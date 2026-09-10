import os
os.environ["CUDA_VISIBLE_DEVICES"] = "2"
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import TensorDataset, DataLoader
from torch.utils.tensorboard import SummaryWriter
from pyhealth.metrics import binary_metrics_fn, multiclass_metrics_fn
import gc
import random
import numpy as np


# 새 서버 환경에서 feature 추출 시 저장되는 split별 파일명
TRAIN_FILENAME = "train_features_labels_subjectids.pt"
VALID_FILENAME = "valid_features_labels_subjectids.pt"
TEST_FILENAME = "test_features_labels_subjectids.pt"

# summary_token 개수 (모든 model 변형에서 embed_num=4로 동일하게 구성됨).
# 저장된 x의 토큰 축(뒤에서 두 번째 축)은 "채널 패치 + summary_token"으로 구성되어 있는데,
# 실제 classifier(linear_probe1/2)는 summary_token만 사용하므로 여기서도 마지막 EMBED_NUM개만 사용한다.
EMBED_NUM = 4


class StructuredProbe(nn.Module):
    """
    main 실험에서 실제로 학습된 linear_probe1/2와 동일한 factorized 구조를 갖되,
    가중치는 불러오지 않고 매번 무작위 초기화해서 새로 학습하는 "공정한" probe.

    (N_time_patch, EMBED_NUM, D)를 통째로 flatten해 거대한 단일 Linear 하나로 매핑하는
    대신, time-patch마다 파라미터를 공유하는 probe1을 먼저 적용한 뒤 그 결과를 모아
    probe2로 최종 클래스 수만큼 줄인다. 이 구조는 실제 head와 동일한 파라미터 공유
    방식이라, 작은 데이터에서도 단일 거대 Linear보다 학습이 잘 될 가능성이 높다.
    원본 forward에서도 probe1/probe2 사이에 활성화 함수가 없으므로 여기서도 넣지 않는다.
    """
    def __init__(self, embed_num, embed_dim, num_time_patch, hidden_dim, num_outputs, dropout_p=0.5):
        super().__init__()
        self.probe1 = nn.Linear(embed_num * embed_dim, hidden_dim)
        self.probe2 = nn.Linear(num_time_patch * hidden_dim, num_outputs)
        self.dropout = nn.Dropout(p=dropout_p)

    def forward(self, x):
        # x: (B, N_time_patch, EMBED_NUM, D)
        B, N, E, D = x.shape
        h = self.dropout(x.reshape(B, N, E * D))
        h = self.probe1(h)      # (B, N_time_patch, hidden_dim)
        h = h.reshape(B, -1)    # (B, N_time_patch * hidden_dim)
        h = self.probe2(h)      # (B, num_outputs)
        return h


def compute_loss(logits, y, output_type):
    """
    output_type에 따라 적절한 손실 함수를 계산합니다.
    - multiclass: 다중 분류 (Cross Entropy)
    - multilabel: 다중 레이블 분류 (Binary Cross Entropy with Logits)
    - regression: 회귀 (MSE)
    """
    if output_type == "multiclass":
        loss = F.cross_entropy(logits, y, reduction="mean")
    elif output_type == "multilabel":
        # num_outputs=1인 이진 분류의 경우 logits는 (B, 1)인데 y는 (B,)로 들어오므로
        # shape을 logits에 맞춰준다 (multilabel 다중 출력인 경우는 원래도 (B, num_outputs)라 no-op).
        loss = F.binary_cross_entropy_with_logits(logits, y.float().view_as(logits), reduction="mean")
    elif output_type == "regression":
        loss = F.mse_loss(logits, y, reduction="mean")
    else:
        raise NotImplementedError(f"Unknown output_type: {output_type}")

    return loss


# ==========================================
# 2. PyHealth 기반 메트릭 계산 헬퍼 함수
# ==========================================
def calculate_metrics(all_logits, all_targets, output_type, metrics):
    """
    pyhealth.metrics를 이용해 지정된 메트릭을 계산합니다.
    대상 메트릭: balanced_accuracy, cohen_kappa, f1_weighted
    """
    # 텐서를 넘파이 어레이로 변환
    y_true = all_targets.cpu().numpy()

    if output_type == "multiclass":
        # 멀티클래스는 예측된 확률(Softmax) 또는 로짓을 그대로 전달하고 정답은 1차원 클래스 인덱스 전달
        y_pred = F.softmax(all_logits, dim=1).cpu().numpy()

        metrics = multiclass_metrics_fn(
            y_true,
            y_pred,
            metrics=metrics
        )
    elif output_type == "multilabel":
        # 바이너리/멀티레이블은 시그모이드를 통과한 확률값을 전달
        y_pred = torch.sigmoid(all_logits).cpu().numpy()
        if y_pred.ndim > 1 and y_pred.shape[-1] == 1:
            # num_outputs=1인 이진 분류의 경우 (N, 1) -> (N,)으로 맞춰준다
            y_pred = y_pred.squeeze(-1)

        metrics = binary_metrics_fn(
            y_true,
            y_pred,
            metrics=metrics
        )
    else:
        raise NotImplementedError(f"Metrics for {output_type} are not configured.")

    return metrics


def load_split_dataset(feature_path):
    """
    미리 train/valid/test로 나뉘어 저장된 피처(.pt)를 불러와 TensorDataset으로 반환합니다.

    저장된 x는 [N, N_time_patch, mC+EMBED_NUM, num_features] 형태로, 뒤에서 두 번째 축이
    "채널 패치 + summary_token" 토큰 축입니다. 실제 classifier(linear_probe1/2)는 이 중
    summary_token(마지막 EMBED_NUM개)만 사용하므로 여기서도 동일하게 마지막 EMBED_NUM개만
    남깁니다. StructuredProbe가 (N_time_patch, EMBED_NUM, D) 구조를 그대로 필요로 하므로
    여기서는 flatten하지 않고 구조를 유지한 채 반환합니다.
    (이미 summary_token만 저장된 경우에도 슬라이싱은 그대로 전체 축을 선택하므로 안전하다.)
    """
    if not os.path.exists(feature_path):
        print(f"Error: {feature_path} 경로에 파일이 존재하지 않습니다. 스킵합니다.")
        exit()

    print(f"Loading features from {feature_path}...")
    data = torch.load(feature_path)

    x = data['x'].float()
    if x.dim() > 2:
        x = x[..., -EMBED_NUM:, :]
    y = data['y']

    return TensorDataset(x, y)


# ==========================================
# 2. 레이어별 단일 실험 실행 함수
# ==========================================
def run_layer_experiment(
    date,
    dataset_path,
    model,
    layer_num,
    output_type="multiclass",
    num_outputs=10,
    metrics=None,
    epochs=50,
    batch_size=64,
    lr=1e-3,
    weight_decay=1e-4,
    patience=10,  # [사수 추가] Early stopping을 위한 patience 값 (기본 10 에폭)
    hidden_dim=16,  # StructuredProbe의 probe1 출력 차원 (원본 linear_probe1과 동일하게 기본 16)
    dropout_p=0.5,  # 원본 head 학습 때와 동일하게 기본 0.5
):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"\n>>> [Layer {layer_num}] 실험 시작 (Device: {device})")

    # 2.1 파일 경로 설정 및 데이터 로드 (train/valid/test가 이미 분리되어 저장되어 있음)
    layer_dir = f"{dataset_path}/{model}_model/SUB1_Model/layer_{layer_num}"
    train_path = os.path.join(layer_dir, TRAIN_FILENAME)
    valid_path = os.path.join(layer_dir, VALID_FILENAME)
    test_path = os.path.join(layer_dir, TEST_FILENAME)

    train_dataset = load_split_dataset(train_path)
    val_dataset = load_split_dataset(valid_path)
    test_dataset = load_split_dataset(test_path)
    print(f"Train: {len(train_dataset)}, Valid: {len(val_dataset)}, Test: {len(test_dataset)}")

    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=False)
    test_loader = DataLoader(test_dataset, batch_size=batch_size, shuffle=False)

    # 2.3 모델 및 옵티마이저 선언
    # (N_time_patch, EMBED_NUM, D)는 layer_num/dataset마다 달라질 수 있으므로 동적으로 계산
    _, num_time_patch, embed_num, embed_dim = train_dataset.tensors[0].shape
    print(f"[Layer {layer_num}] feature shape = (N_time_patch={num_time_patch}, "
        f"EMBED_NUM={embed_num}, D={embed_dim})")
    probe = StructuredProbe(
        embed_num=embed_num,
        embed_dim=embed_dim,
        num_time_patch=num_time_patch,
        hidden_dim=hidden_dim,
        num_outputs=num_outputs,
        dropout_p=dropout_p,
    ).to(device)

    optimizer = torch.optim.Adam(probe.parameters(), lr=lr, weight_decay=weight_decay)

    # 2.4 TensorBoard SummaryWriter 설정
    # 각 레이어별로 개별 폴더가 생성되어 Tensorboard 상에서 멀티 그래프 비교 가능
    log_dir = f"/data/log/BCIC2A_log/{date}/{model}_model/SUB_1_Model/layer_{layer_num}"
    writer = SummaryWriter(log_dir=log_dir)

    best_val_balanced_acc = float("-inf")

    early_stopping_counter = 0

    # 2.5 학습 및 검증 루프
    for epoch in range(epochs):
        # --- Train Phase ---
        probe.train()
        train_loss = 0.0

        for batch_X, batch_y in train_loader:
            batch_X, batch_y = batch_X.to(device), batch_y.to(device)

            optimizer.zero_grad()
            logits = probe(batch_X)
            loss = compute_loss(logits, batch_y, output_type)
            loss.backward()
            optimizer.step()

            train_loss += loss.item() * batch_X.size(0)

        train_loss /= len(train_loader.dataset)
        writer.add_scalar("Loss/Train", train_loss, epoch)

        # --- Validation Phase ---
        probe.eval()
        val_loss = 0.0
        val_logits_list = []
        val_targets_list = []

        with torch.no_grad():
            for batch_X, batch_y in val_loader:
                batch_X, batch_y = batch_X.to(device), batch_y.to(device)
                logits = probe(batch_X)
                loss = compute_loss(logits, batch_y, output_type)
                val_loss += loss.item() * batch_X.size(0)

                val_logits_list.append(logits)
                val_targets_list.append(batch_y)

        val_loss /= len(val_loader.dataset)
        writer.add_scalar("Loss/Valid", val_loss, epoch)

        # 모든 배치 결과 병합 후 pyhealth 메트릭 계산
        val_logits_all = torch.cat(val_logits_list, dim=0)
        val_targets_all = torch.cat(val_targets_list, dim=0)
        val_metrics = calculate_metrics(val_logits_all, val_targets_all, output_type, metrics)

        # TensorBoard에 새 메트릭 기록
        writer.add_scalar("Balanced_Accuracy/Valid", val_metrics["balanced_accuracy"], epoch)
        writer.add_scalar("Cohen_Kappa/Valid", val_metrics["cohen_kappa"], epoch)
        writer.add_scalar("Weighted_F1/Valid", val_metrics["f1_weighted"], epoch)

        # Validation Balanced Accuracy 최고점 기준 best_model 저장
        current_val_balanced_acc = val_metrics["balanced_accuracy"]
        if current_val_balanced_acc > best_val_balanced_acc:
            best_val_balanced_acc = current_val_balanced_acc

            early_stopping_counter = 0

            checkpoint_dict = {
                "epoch": epoch + 1,
                "model_state_dict": probe.state_dict(),
                "optimizer_state_dict": optimizer.state_dict(),
            }
            ckpt_save_path = os.path.join(log_dir, "best_model.ckpt")
            torch.save(checkpoint_dict, ckpt_save_path)
            print(f"--> Epoch {epoch+1}: 최고 Balanced Accuracy 갱신! ({best_val_balanced_acc:.4f}) -> 체크포인트 저장")
        else:
            early_stopping_counter += 1

        # 5 에폭마다 간결하게 로그 출력
        if (epoch + 1) % 5 == 0:
            print(f"Epoch [{epoch+1}/{epochs}] | Train Loss: {train_loss:.4f} | Valid Loss: {val_loss:.4f} | "
                f"Valid B_Acc: {val_metrics['balanced_accuracy']:.4f} | "
                f"Valid Kappa: {val_metrics['cohen_kappa']:.4f} | "
                f"Valid W_F1: {val_metrics['f1_weighted']:.4f}")

        if early_stopping_counter >= patience:
            print(f"\n[Early Stopping Triggered] {patience} 에폭 동안 성능 개선이 없어 학습을 조기 종료합니다. (현재 Epoch: {epoch+1})")
            break  # for epoch 루프를 즉시 탈출!

    # 3.3 최종 에폭 모델 저장 (final_model.ckpt)
    final_checkpoint_dict = {
        "epoch": epochs,
        "model_state_dict": probe.state_dict(),
        "optimizer_state_dict": optimizer.state_dict()
    }
    final_ckpt_path = os.path.join(log_dir, "final_model.ckpt")
    torch.save(final_checkpoint_dict, final_ckpt_path)
    print(f"===> [Layer {layer_num}] 학습 완료! 최종 모델 저장 완료 -> {final_ckpt_path}")

    # 2.6 최종 Test Phase
    print(f"===> [Layer {layer_num}] 최적의 가중치 복원 후 Test 세트 평가 시작...")
    best_ckpt = torch.load(os.path.join(log_dir, "best_model.ckpt"), map_location=device)
    probe.load_state_dict(best_ckpt["model_state_dict"])
    probe.eval()

    test_loss = 0.0
    test_logits_list = []
    test_targets_list = []

    with torch.no_grad():
        for batch_X, batch_y in test_loader:
            batch_X, batch_y = batch_X.to(device), batch_y.to(device)
            logits = probe(batch_X)
            loss = compute_loss(logits, batch_y, output_type)
            test_loss += loss.item() * batch_X.size(0)

            test_logits_list.append(logits)
            test_targets_list.append(batch_y)

    test_loss /= len(test_loader.dataset)
    test_logits_all = torch.cat(test_logits_list, dim=0)
    test_targets_all = torch.cat(test_targets_list, dim=0)
    test_metrics = calculate_metrics(test_logits_all, test_targets_all, output_type, metrics)

    print(f"=====> [Layer {layer_num}] Test 결과 | Test Loss: {test_loss:.4f} | "
        f"Test B_Acc: {test_metrics['balanced_accuracy']:.4f} | "
        f"Test Kappa: {test_metrics['cohen_kappa']:.4f} | "
        f"Test W_F1: {test_metrics['f1_weighted']:.4f}")

    # TensorBoard에 하이퍼파라미터 및 최종 테스트 스코어 기록
    hparams_dict = {
        "layer_num": layer_num,
        "lr": lr,
        "weight_decay": weight_decay,
    }
    metrics_dict = {
        "hparam/test_loss": test_loss,
        "hparam/test_balanced_accuracy": test_metrics["balanced_accuracy"],
        "hparam/test_cohen_kappa": test_metrics["cohen_kappa"],
        "hparam/test_weighted_f1": test_metrics["f1_weighted"]
    }
    writer.add_hparams(
        hparam_dict=hparams_dict,
        metric_dict=metrics_dict,
        run_name="."
    )

    writer.close()


# ==========================================
# 3. 메인 실험 실행 컨트롤러
# ==========================================
if __name__ == "__main__":
    def seed_torch(seed=1029):
        random.seed(seed)
        os.environ['PYTHONHASHSEED'] = str(seed) # 为了禁止hash随机化，使得实验可复现
        np.random.seed(seed)
        torch.manual_seed(seed)
        torch.cuda.manual_seed(seed)
        # torch.cuda.manual_seed_all(seed) # if you are using multi-GPU.
        torch.backends.cudnn.benchmark = False
        torch.backends.cudnn.deterministic = True
    seed_torch(7)

    # 설정값 정의
    DATE = f'260716_LINEAR_WISE_LP'
    DATASET_PATH = "/data/dataset/BCIC-2A/layer_wise/entire" # 사용 중인 데이터셋 폴더명 입력
    MODEL = 'linear_probe'  # linear_probe finetune pearl LORA DORA VERA
    OUTPUT_TYPE = "multiclass"          # multiclass, multilabel, regression 중 선택
    NUM_OUTPUTS = 4                     # 분류 클래스 수 혹은 회귀 타겟 차원 수
    METRICS = ["accuracy", "balanced_accuracy", "cohen_kappa", "f1_weighted"]
    EPOCHS = 120
    BATCH_SIZE = 128

    # 1번 레이어부터 8번 레이어까지 순차적으로 훈련 및 평가 진행
    for layer in range(1, 9):
        run_layer_experiment(
            date=DATE,
            dataset_path=DATASET_PATH,
            model=MODEL,
            layer_num=layer,
            output_type=OUTPUT_TYPE,
            num_outputs=NUM_OUTPUTS,
            metrics=METRICS,
            epochs=EPOCHS,
            batch_size=BATCH_SIZE,
            patience=10,
        )

        gc.collect()               # CPU 파이썬 가비지 컬렉션 가동
        torch.cuda.empty_cache()   # GPU 미사용 캐시 메모리 확보

    print("\n[모든 레이어 실험 종료] 터미널에 'tensorboard --logdir=runs'를 입력해 결과를 확인하세요.")
