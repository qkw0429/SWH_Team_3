"""
대조(control) 실험: layer-wise MLP probe 대신, main 실험에서 실제로 학습된
linear_probe1 / linear_probe2 헤드를 그대로 불러와 SUB1 fold의 feature에 적용했을 때도
main 실험과 같은 순위(예: dynamic pearl 우세)가 재현되는지 확인한다.

이 head는 encoder(및 prompt/adapter)와 함께 method별로 따로 학습된 파라미터이므로,
반드시 평가하려는 method 자신의 checkpoint에서 불러와야 한다. 다른 method의 head를
가져다 쓰면 입력 분포가 맞지 않아 의미 있는 비교가 되지 않는다.

원래 학습된 forward 경로(LitEEGPTCausal, 주석 처리되지 않은 버전)를 그대로 재현한다:
    z = target_encoder(x, chan_ids)               # (B, N_time_patch, EMBED_NUM, D)
    z = norm(z)                                   # target_encoder.norm (LayerNorm, method별 학습됨)
    h = z.flatten(2)                              # (B, N_time_patch, EMBED_NUM*D)
    h = linear_probe1(h)                          # (B, N_time_patch, 16)
    h = h.flatten(1)                              # (B, N_time_patch*16)
    h = linear_probe2(h)                          # (B, num_outputs)

LinearWithConstraint은 학습 중 weight norm을 clamp하는 제약이지만, 이미 수렴된
가중치를 그대로 불러와 추론만 하는 상황에서는 plain nn.Linear와 동일하게 동작한다.

이 head는 layer 8의 summary_token 분포에 맞춰 학습되었다. 다른 layer에도 적용해
전체 layer를 훑어볼 수 있지만, layer 8이 아닌 결과는 "그 layer의 feature 품질"과
"head의 분포 불일치로 인한 성능 저하"가 뒤섞여 있어 method 간 feature 품질 비교의
근거로 쓰기 어렵다 — layer 8 결과만 원래 목적(fresh MLP probe vs 학습된 head 비교)에
안전하게 쓸 수 있고, 나머지는 참고용 진단 정보로 취급해야 한다.
"""
import os
os.environ.setdefault("CUDA_VISIBLE_DEVICES", "2")
import torch
import torch.nn as nn
from torch.utils.data import DataLoader

from train_mlp_probe import EMBED_NUM, TEST_FILENAME, calculate_metrics, compute_loss


def find_key(state_dict, suffix):
    """state_dict에서 지정한 suffix로 끝나는 키를 찾는다 (prefix가 붙어 있어도 대응)."""
    matches = [k for k in state_dict.keys() if k.endswith(suffix)]
    if len(matches) == 0:
        raise KeyError(f"'{suffix}'로 끝나는 키를 checkpoint에서 찾지 못했습니다.")
    if len(matches) > 1:
        raise KeyError(f"'{suffix}'로 끝나는 키가 여러 개 발견되었습니다: {matches}")
    return matches[0]


def load_probe_heads(ckpt_path, device):
    """checkpoint에서 method 자신의 linear_probe1 / linear_probe2 가중치를 불러온다."""
    checkpoint = torch.load(ckpt_path, map_location=device)
    state_dict = checkpoint["state_dict"] if "state_dict" in checkpoint else checkpoint

    w1_key = find_key(state_dict, "linear_probe1.weight")
    b1_key = find_key(state_dict, "linear_probe1.bias")
    w2_key = find_key(state_dict, "linear_probe2.weight")
    b2_key = find_key(state_dict, "linear_probe2.bias")

    w1, b1 = state_dict[w1_key], state_dict[b1_key]
    w2, b2 = state_dict[w2_key], state_dict[b2_key]

    linear_probe1 = nn.Linear(w1.shape[1], w1.shape[0])
    linear_probe1.weight.data.copy_(w1)
    linear_probe1.bias.data.copy_(b1)

    linear_probe2 = nn.Linear(w2.shape[1], w2.shape[0])
    linear_probe2.weight.data.copy_(w2)
    linear_probe2.bias.data.copy_(b2)

    return linear_probe1.to(device).eval(), linear_probe2.to(device).eval()


def load_final_norm(ckpt_path, device):
    """
    checkpoint에서 target_encoder의 최종 LayerNorm(self.norm) 가중치를 불러온다.
    forward()에서 summary_token을 자른 뒤 linear_probe1에 넣기 전에 이 norm을
    한 번 통과시키므로, 이것도 method별로 따로 학습된 파라미터라서 반드시
    평가하려는 method 자신의 checkpoint에서 불러와야 한다.
    """
    checkpoint = torch.load(ckpt_path, map_location=device)
    state_dict = checkpoint["state_dict"] if "state_dict" in checkpoint else checkpoint

    w_key = find_key(state_dict, "norm.weight")
    b_key = find_key(state_dict, "norm.bias")
    w, b = state_dict[w_key], state_dict[b_key]

    norm = nn.LayerNorm(w.shape[0], eps=1e-6)
    norm.weight.data.copy_(w)
    norm.bias.data.copy_(b)

    return norm.to(device).eval()


def load_summary_token_features(feature_path):
    """
    raw feature(.pt)에서 summary_token 부분만 잘라 (N, N_time_patch, EMBED_NUM, D) 형태로
    반환한다. train_mlp_probe.load_split_dataset과 달리 flatten하지 않고 구조를 유지한다.
    """
    if not os.path.exists(feature_path):
        raise FileNotFoundError(f"{feature_path} 경로에 파일이 존재하지 않습니다.")

    data = torch.load(feature_path)
    x = data["x"].float()
    if x.dim() > 2:
        x = x[..., -EMBED_NUM:, :]
    y = data["y"]
    return x, y


def head_forward(x, norm, linear_probe1, linear_probe2):
    """x: (B, N_time_patch, EMBED_NUM, D) -> logits: (B, num_outputs)"""
    x = norm(x)  # forward()에서 summary_token을 자른 뒤 linear_probe1 전에 통과시키는 LayerNorm
    B, N, E, D = x.shape
    h = x.reshape(B, N, E * D)
    h = linear_probe1(h)
    h = h.reshape(B, -1)
    h = linear_probe2(h)
    return h


def run_head_control_experiment(
    method_name,
    norm,
    linear_probe1,
    linear_probe2,
    dataset_path,
    model,
    layer_num,
    device,
    output_type="multiclass",
    metrics=None,
    batch_size=64,
):
    """
    method 자신의 학습된 head(linear_probe1/2)를 SUB1 fold의 지정 layer feature에
    적용해 test set 성능을 계산한다.

    주의: 이 head는 layer 8의 summary_token 분포에 맞춰 학습되었다. layer 8이 아닌
    다른 layer에 적용한 결과는 "그 layer의 feature 품질"과 "head의 분포 불일치로 인한
    성능 저하"가 뒤섞여 있어, method 간 feature 품질을 비교하는 근거로 쓰기에는
    주의가 필요하다. layer 8 결과만 원래 목적(fresh MLP probe vs 학습된 head 비교)에
    안전하게 쓸 수 있고, 나머지 layer는 참고용 진단 정보로 취급한다.
    """
    print(f"\n>>> [{method_name} / Layer {layer_num}] 학습된 head 대조 실험 시작 (Device: {device})")

    layer_dir = f"{dataset_path}/{model}_model/SUB1_Model/layer_{layer_num}"
    test_path = os.path.join(layer_dir, TEST_FILENAME)
    x_test, y_test = load_summary_token_features(test_path)
    print(f"Test: {len(y_test)} samples, feature shape = {tuple(x_test.shape)}")

    test_logits_list = []
    test_targets_list = []
    with torch.no_grad():
        for start in range(0, len(y_test), batch_size):
            batch_x = x_test[start:start + batch_size].to(device)
            batch_y = y_test[start:start + batch_size].to(device)
            logits = head_forward(batch_x, norm, linear_probe1, linear_probe2)
            test_logits_list.append(logits)
            test_targets_list.append(batch_y)

    test_logits_all = torch.cat(test_logits_list, dim=0)
    test_targets_all = torch.cat(test_targets_list, dim=0)
    test_loss = compute_loss(test_logits_all, test_targets_all, output_type).item()
    test_metrics = calculate_metrics(test_logits_all, test_targets_all, output_type, metrics)

    print(f"=====> [{method_name} / Layer {layer_num}] Test 결과 | Test Loss: {test_loss:.4f} | "
        f"Test B_Acc: {test_metrics['balanced_accuracy']:.4f} | "
        f"Test Kappa: {test_metrics['cohen_kappa']:.4f} | "
        f"Test W_F1: {test_metrics['f1_weighted']:.4f}")

    return test_metrics


if __name__ == "__main__":
    DATASET_PATH = "/data/dataset/BCIC-2A/layer_wise/entire"
    OUTPUT_TYPE = "multiclass"
    METRICS = ["accuracy", "balanced_accuracy", "cohen_kappa", "f1_weighted"]
    LAYER_NUMS = list(range(1, 9))
    HEAD_TRAINED_LAYER = 8  # linear_probe1/2가 실제로 학습된 layer (이 layer 결과만 method 비교에 안전)

    # method 이름 -> (dataset_path의 {model}_model 폴더명, 학습된 checkpoint 경로)
    METHOD_CONFIGS = {
        "linear_probe": (
            "linear_probe",
            "/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_4_LP_NO_CHAN_CONV_csv/subject1/epoch=82_step=5395_best_checkpoint.ckpt",
        ),
        "pearl": (
            "pearl",
            "/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_5_PEARL_csv/subject1/epoch=48_best_checkpoint.ckpt",
        ),
        "dynamic_pearl": (
            "dynamic_pearl",
            "/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_11_PEARL_MULTI_SCALE_DYNAMIC_CONV_KERNEL_6,11,16,21,26,31,36,41_REDUCTION_2_csv/subject1/epoch=74_best_checkpoint.ckpt",
        ),
        "vera": (
            "VERA",
            "/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_8_VERA_only_qkv_RANK_256_NO_CHAN_CONV_csv/subject1/epoch=74_best_checkpoint.ckpt",
        ),
    }

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    # method별 norm/head는 한 번만 불러오고, layer마다 재사용한다.
    results = {method_name: {} for method_name in METHOD_CONFIGS}
    for method_name, (model_dir, ckpt_path) in METHOD_CONFIGS.items():
        norm = load_final_norm(ckpt_path, device)
        linear_probe1, linear_probe2 = load_probe_heads(ckpt_path, device)
        for layer_num in LAYER_NUMS:
            results[method_name][layer_num] = run_head_control_experiment(
                method_name=method_name,
                norm=norm,
                linear_probe1=linear_probe1,
                linear_probe2=linear_probe2,
                dataset_path=DATASET_PATH,
                model=model_dir,
                layer_num=layer_num,
                device=device,
                output_type=OUTPUT_TYPE,
                metrics=METRICS,
            )

    print("\n[학습된 head 대조 실험 요약] (★ = head가 실제로 학습된 layer, 이 layer만 method 비교에 안전함)")
    header = "method".ljust(15) + "".join(
        f"L{layer_num}{'★' if layer_num == HEAD_TRAINED_LAYER else ' '}".rjust(12)
        for layer_num in LAYER_NUMS
    )
    print(header)
    for method_name, per_layer in results.items():
        row = method_name.ljust(15) + "".join(
            f"{per_layer[layer_num]['balanced_accuracy']:.4f}".rjust(12) for layer_num in LAYER_NUMS
        )
        print(row)
