import random 
import os
os.environ["CUDA_VISIBLE_DEVICES"] = "0"
import argparse
import torch
from torch import nn
import pytorch_lightning as pl

from functools import partial
import numpy as np
import tqdm
from pytorch_lightning import loggers as pl_loggers
import torch.nn.functional as F
import json
import gc


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


from Modules.models.EEGPT_mcae_for_linear_wise_invest import EEGTransformer, EEGTransformerReprogramLwc1d, EEGTransformerReprogramLwc1d_MULTI_SCALE_DYNAMIC_CONV

from Modules.Network.utils import Conv1dWithConstraint, LinearWithConstraint
from utils_eval import get_metrics



device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')




ENTIRE_OR_ONLY_TEST = 'ENTIRE'
TRAIN_OR_VALID_OR_TEST = 'VALID'

if ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
    TRAIN_OR_VALID_OR_TEST = 'TEST'


NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL = 'DYNAMICPEARL'



POOLING = []

# summary_token 개수 (모든 model 변형에서 embed_num=4로 동일하게 구성됨).
# forward()에서 "x = x[:, -summary_token.shape[1]:, :]"로 최종 classifier(linear_probe1/2)에
# 전달되는 부분과 동일한 토큰만 남기기 위해 저장 직전에 이 값으로 슬라이싱한다.
EMBED_NUM = 4

SUB = 1
ENTIRE_SUB = 8


FRONT =  100
KERNEL = 40
STRIDE = 20
HEADS = 1
CONV_SOFTMAX = False
CONV_BIAS = True
CONV_DROP = 0.5


LORA_RANK = 8

VERA_RANK = 256

REDUCTION = 2

KERNEL_SIZES = (6, 11, 16, 21, 26, 31, 36, 41)


if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DORA':
    USE_DORA = True
else:
    USE_DORA = False




from peft import LoraConfig, get_peft_model, VeraConfig

def apply_lora_to_eeg_encoder(model, lora_alpha=8, lora_dropout=0.05):
    """
    LitEEGPTCausal 모델의 target_encoder.blocks 내부에만 제한적으로 LoRA를 적용합니다.
    """
    
    # 1. LoRA 설정 (정규표현식 사용)
    # 정규표현식을 해석하자면:
    # '.*' : 앞에 어떤 경로가 오든 상관없음 (예: base_model.model...)
    # 'target_encoder\.blocks' : 우리가 타겟팅하는 특정 모듈 경로
    # '.*' : 블록 내부의 어떤 깊이든 상관없음 (예: .0.attn, .1.mlp 등)
    # '(qkv|proj|fc1|fc2)' : 실제 LoRA를 붙일 대상 Linear 레이어들의 이름
    # 주의: 본인의 Block 내부 Linear 레이어 이름에 맞춰 괄호 안의 이름을 수정해야 합니다.
    # target_regex = r".*target_encoder\.blocks.*(?:qkv|proj|fc1|fc2)"
    # target_regex = r".*blocks.*(?:qkv|proj|fc1|fc2)"
    target_regex = r".*blocks.*(?:qkv)"
    
    lora_config = LoraConfig(
        r=LORA_RANK,
        lora_alpha=lora_alpha,
        target_modules=target_regex,
        # modules_to_save=["chan_conv", "linear_probe1", "linear_probe2"],
        lora_dropout=lora_dropout,
        bias="none",
        use_dora=USE_DORA
        # task_type="CAUSAL_LM" # Task 목적에 맞게 변경 (분류면 SEQ_CLS 등)
    )

    # 2. 모델에 LoRA 래핑
    # lora_model = get_peft_model(model, lora_config)
    model.target_encoder = get_peft_model(model.target_encoder, lora_config)

    # 3. 🔥 핵심: 타겟 원본 가중치를 제외한 나머지 전부 학습 활성화
    for name, param in model.target_encoder.named_parameters():
        # peft 래핑 후 원본 가중치는 'base_layer'로 감춰집니다.
        # 이 원본 가중치들은 동결을 유지해야 LoRA의 의미가 있습니다.
        # if "base_layer" in name or "original_module" in name:
        #     param.requires_grad = False
        # else:
        #     # LoRA 파라미터 및 그 외 모든 모듈(LayerNorm, Embeddings, Positional Encoding 등) 학습 활성화
        #     param.requires_grad = True
    # 1) LoRA 원본 가중치는 반드시 동결
        if "base_layer" in name or "original_module" in name:
            param.requires_grad = False
            
        # 2) 명시적으로 동결하고 싶은 레이어 지정 (여기에 proj, fc1, fc2 추가)
        elif any(keyword in name for keyword in ["proj", "fc1", "fc2", "norm"]):
            param.requires_grad = False
            
        # 3) 나머지 (LoRA의 A/B 가중치, LayerNorm, Embeddings 등)는 학습 활성화
        else:
            param.requires_grad = True
    
    # 3. 학습 가능한 파라미터 수 확인 (디버깅/보고용)
    # lora_model.print_trainable_parameters()
    # model.target_encoder.print_trainable_parameters()
    
    return model




def apply_vera_to_eeg_encoder(model, vera_rank=128, vera_dropout=0.0):
    """
    LitEEGPTCausal 모델의 target_encoder.blocks 내부에만 제한적으로 LoRA를 적용합니다.
    """
    
    # 1. LoRA 설정 (정규표현식 사용)
    # 정규표현식을 해석하자면:
    # '.*' : 앞에 어떤 경로가 오든 상관없음 (예: base_model.model...)
    # 'target_encoder\.blocks' : 우리가 타겟팅하는 특정 모듈 경로
    # '.*' : 블록 내부의 어떤 깊이든 상관없음 (예: .0.attn, .1.mlp 등)
    # '(qkv|proj|fc1|fc2)' : 실제 LoRA를 붙일 대상 Linear 레이어들의 이름
    # 주의: 본인의 Block 내부 Linear 레이어 이름에 맞춰 괄호 안의 이름을 수정해야 합니다.
    # target_regex = r".*target_encoder\.blocks.*(?:qkv|proj|fc1|fc2)"
    # target_regex = r".*blocks.*(?:qkv|proj|fc1|fc2)"
    target_regex = r".*blocks.*(?:qkv)"
    
    vera_config = VeraConfig(
        r=VERA_RANK,
        target_modules=target_regex,
        vera_dropout=vera_dropout,
        bias="none",
        # task_type="CAUSAL_LM" # Task 목적에 맞게 주석 해제 가능
    )

    # 2. 모델에 VeRA 래핑
    model.target_encoder = get_peft_model(model.target_encoder, vera_config)

    # 3. 🔥 핵심: blocks 내부와 외부를 분리하여 학습 파라미터 정밀 제어
    for name, param in model.target_encoder.named_parameters():
        if "blocks" in name:
            # [CASE 1] blocks 내부에 존재하는 파라미터인 경우
            # 오직 vera_lambda_b와 vera_lambda_d만 학습을 활성화합니다.
            if "vera_lambda_b" in name or "vera_lambda_d" in name:
                param.requires_grad = True
            else:
                # blocks 내부의 LayerNorm, 원래 weight/bias, vera_A, vera_B 등은 모두 동결
                param.requires_grad = False
        else:
            # [CASE 2] blocks 외부에 존재하는 파라미터인 경우 (summary_token, embedding 등)
            # 기본적으로 학습을 활성화하되, 혹시 모를 PEFT 원본 가중치 감춤 태그가 있다면 동결합니다.
            if any(keyword in name for keyword in ["base_layer", "original_module", "vera_A", "vera_B"]):
                param.requires_grad = False
            else:
                param.requires_grad = True
    
    # 3. 학습 가능한 파라미터 수 확인 (디버깅/보고용)
    # lora_model.print_trainable_parameters()
    # model.target_encoder.print_trainable_parameters()
    
    return model






class LitEEGPTCausal(pl.LightningModule):

    #def __init__(self, load_path="../checkpoint/eegpt_mcae_58chs_4s_large4E.ckpt"):
    def __init__(self, load_path="/data/EEGPT-main/checkpoint/eegpt_mcae_58chs_4s_large4E.ckpt"):
        super().__init__()
        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'FINETUNE':
            self.chans_num = 19
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL != 'FINETUNE':
            self.chans_num = 22
        # init model
        
        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'FINETUNE':
            # use_channels_names = ['FZ', 'FC3', 'FC1', 'FCZ', 'FC2', 'FC4', 'C5', 'C3', 'C1', 'CZ', 'C2', 'C4', 'C6', 'CP3', 'CP1', 'CPZ', 'CP2', 'CP4', 'P1', 'PZ', 'P2', 'POZ']
            use_channels_names = [      
               'FP1', 'FP2',
        'F7', 'F3', 'FZ', 'F4', 'F8',
        'T7', 'C3', 'CZ', 'C4', 'T8',
        'P7', 'P3', 'PZ', 'P4', 'P8',
                'O1', 'O2' ]
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL != 'FINETUNE':
            use_channels_names = ['FZ', 'FC3', 'FC1', 'FCZ', 'FC2', 'FC4', 'C5', 'C3', 'C1', 'CZ', 'C2', 'C4', 'C6', 'CP3', 'CP1', 'CPZ', 'CP2', 'CP4', 'P1', 'PZ', 'P2', 'POZ']
        
        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DYNAMICPEARL':
            target_encoder = EEGTransformerReprogramLwc1d_MULTI_SCALE_DYNAMIC_CONV(
            # (front 100) img_size 1024->1088  16*16->17*16로 변경
            # (front 200) img_size 1024->1216  16*16->19*16로 변경
            # (front 50) img_size 1024->1024  16*16->16*16로 변경
            # (front 20) img_size 1024->1024  16*16->16*16로 변경
            img_size=[self.chans_num, 1024],
            patch_size=32*2,
            embed_num=4,
            embed_dim=512,
            depth=8,
            num_heads=8,
            mlp_ratio=4.0,
            drop_rate=0.0,
            attn_drop_rate=0.0,
            drop_path_rate=0.0,
            init_std=0.02,
            qkv_bias=True, 
            norm_layer=partial(nn.LayerNorm, eps=1e-6),
            front=FRONT,
            channel_num = len(use_channels_names),
            kernel_sizes=KERNEL_SIZES,
            num_head=HEADS,
            conv_weight_softmax=CONV_SOFTMAX,
            conv_bias=CONV_BIAS,
            conv_dropout=CONV_DROP,
            stride=STRIDE,
            # num_kernels=NUM_KERNELS,
            reduction=REDUCTION,
            # use_traditional_conv=USE_TRADITIONAL_CONV
        )
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'PEARL':
            target_encoder = EEGTransformerReprogramLwc1d(
            # (front 100) img_size 1024->1088  16*16->17*16로 변경
            # (front 200) img_size 1024->1216  16*16->19*16로 변경
            # (front 50) img_size 1024->1024  16*16->16*16로 변경
            # (front 20) img_size 1024->1024  16*16->16*16로 변경
            img_size=[self.chans_num, 1024],
            patch_size=32*2,
            embed_num=4,
            embed_dim=512,
            depth=8,
            num_heads=8,
            mlp_ratio=4.0,
            drop_rate=0.0,
            attn_drop_rate=0.0,
            drop_path_rate=0.0,
            init_std=0.02,
            qkv_bias=True, 
            norm_layer=partial(nn.LayerNorm, eps=1e-6),
            front=FRONT,
            channel_num = len(use_channels_names),
            kernel_size=KERNEL,
            num_head=HEADS,
            conv_weight_softmax=CONV_SOFTMAX,
            conv_bias=CONV_BIAS,
            conv_dropout=CONV_DROP,
            stride=STRIDE,
            # use_traditional_conv=USE_TRADITIONAL_CONV
        )
        else:
            target_encoder = EEGTransformer(
                img_size=[self.chans_num, 1024],
                patch_size=32*2,
                embed_num=4,
                embed_dim=512,
                depth=8,
                num_heads=8,
                mlp_ratio=4.0,
                drop_rate=0.0,
                attn_drop_rate=0.0,
                drop_path_rate=0.0,
                init_std=0.02,
                qkv_bias=True, 
                norm_layer=partial(nn.LayerNorm, eps=1e-6))
        
        
            
        self.target_encoder = target_encoder
        self.chans_id       = target_encoder.prepare_chan_ids(use_channels_names)
        # print(f"self.chans_id = {self.chans_id}")
        
        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL not in ['LORA', 'DORA', 'VERA']:
            # -- load checkpoint
            pretrain_ckpt = torch.load(load_path)
            target_encoder_stat = {}
            for k,v in pretrain_ckpt['state_dict'].items():
                if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DYNAMICPEARL':
                    if k.startswith("target_encoder."):
                        new_key = k[15:]
                        if new_key.startswith("dynamic_conv."):
                            new_key = "multiscale_dynamic_conv." + new_key[len("dynamic_conv."):]
                        target_encoder_stat[new_key] = v
                else:
                    if k.startswith("target_encoder."):
                        target_encoder_stat[k[15:]]=v
            self.target_encoder.load_state_dict(target_encoder_stat)
            # if LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA != 'PEARL':
            #     self.target_encoder.load_state_dict(target_encoder_stat)
            # elif LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA == 'PEARL':
            #     self.target_encoder.load_state_dict(target_encoder_stat, strict=False) # learnable prompt의 경우 strict=False

        # for blk in self.target_encoder.blocks:
        #     for p in blk.parameters():
        #         p.requires_grad = False
        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'FINETUNE':
            self.chan_conv       = Conv1dWithConstraint(22, self.chans_num, 1, max_norm=1)
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL != 'FINETUNE':
            pass

        self.linear_probe1   =   LinearWithConstraint(2048, 16, max_norm=1)
        self.linear_probe2   =   LinearWithConstraint(16*16, 4, max_norm=0.25)
        
        self.drop           = torch.nn.Dropout(p=0.50)
        
        self.loss_fn        = torch.nn.CrossEntropyLoss()
        self.running_scores = {"train":[], "valid":[], "test":[]}
        self.is_sanity=True
        
    def forward(self, x):
        # print(f"Before channel conv shape = {x.shape}")
        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'FINETUNE':
            x = self.chan_conv(x)
        self.target_encoder.eval()
        features = self.target_encoder(x, self.chans_id.to(x))
        # z = self.target_encoder(x, self.chans_id.to(x))
        
        # h = z.flatten(2)
        
        # h = self.linear_probe1(self.drop(h))
        
        # h = h.flatten(1)
        
        # h = self.linear_probe2(h)
        
        # return x, h
        return features


# load configs
# from utils import *
import scipy.io as sio
from Data_process.utils import EA
from torch.utils.data import Dataset
from sklearn.model_selection import train_test_split
import math
from einops import rearrange
#data_path = "../datasets/downstream/Data/BCIC_2a_0_38HZ"
data_path = "/workspace/EEGPT/dataset/BCIC-2A/preprocessed_data_0_38HZ"



def temporal_interpolation(x, desired_sequence_length, mode='nearest', use_avg=True):
    # print(x.shape)
    # squeeze and unsqueeze because these are done before batching
    if use_avg:
        x = x - torch.mean(x, dim=-2, keepdim=True)
    if len(x.shape) == 2:
        return torch.nn.functional.interpolate(x.unsqueeze(0), desired_sequence_length, mode=mode).squeeze(0)
    # Supports batch dimension
    elif len(x.shape) == 3:
        return torch.nn.functional.interpolate(x, desired_sequence_length, mode=mode)
    else:
        raise ValueError("TemporalInterpolation only support sequence of single dim channels with optional batch")





class eeg_dataset(Dataset):
    '''
    A class need to input the Dataloader in the pytorch.
    '''
    def __init__(self,feature,label,subject_id=None):
        super(eeg_dataset,self).__init__()

        self.x = feature
        self.y = label
        self.s = subject_id

    def __len__(self):
        return len(self.y)

    def __getitem__(self, index):
        return self.x[index], self.y[index], self.s[index]
    
    def get_num_class(self, num_class=[1,1,1,1]):
        res = [[] for i in num_class]
        idxs = [i for i in range(len(self.y))]
        while sum(num_class)>0:
            i = random.choice(idxs)
            label = self.y[i]
            label = int(label)
            if num_class[label]>0:
                num_class[label]-=1
                res[label].append((self.x[i],self.y[i]))
            
        re2= []
        for r in res:
            re2.extend(r)
        x = torch.stack([x[0] for x in re2], dim=0)
        
        y = torch.stack([x[1] for x in re2], dim=0)
        
        return x, y     
    
    def get_num_subject(self, num_class=[1,1,1,1,1,1,1,1]):
        res = [[] for i in num_class]
        idxs = [i for i in range(len(self.y))]
        while sum(num_class)>0:
            i = random.choice(idxs)
            s = self.s[i]
            s = int(s)
            if num_class[s]>0:
                num_class[s]-=1
                res[s].append((self.x[i],self.y[i]))
            
        re2= []
        for r in res:
            re2.extend(r)
        x = torch.stack([x[0] for x in re2], dim=0)
        y = torch.stack([x[1] for x in re2], dim=0)
        
        return x, y        



def get_data_ablation_entire_data(data_path,few_shot_number = 1, is_few_EA = False, target_sample=-1, use_avg=True, use_channels=None):
    all_x = []
    all_y = []
    all_s = []

    # subject_id = 0
    for i in range(1, 10):
        train_path = os.path.join(data_path,r'sub{}_train/Data.mat'.format(i))
        train_data = sio.loadmat(train_path)
    
        test_path = os.path.join(data_path,r'sub{}_test/Data.mat'.format(i))
        test_data = sio.loadmat(test_path)

        R = None
        if is_few_EA is True:
            session_1_x = EA(train_data['x_data'],R)
            session_2_x = EA(test_data['x_data'],R)
        else:
            session_1_x = train_data['x_data']
            session_2_x = test_data['x_data']

        session_1_y = train_data['y_data'].reshape(-1)
        session_2_y = test_data['y_data'].reshape(-1)

        # train_x,valid_x,train_y,valid_y = train_test_split(session_1_x,session_1_y,test_size = 0.1,stratify = session_1_y)
        
        all_x.extend(session_1_x)
        all_y.extend(session_1_y)
        all_s.append(torch.ones((len(session_1_y),))*i)

        all_x.extend(session_2_x)
        all_y.extend(session_2_y)
        all_s.append(torch.ones((len(session_2_y),))*i)

    
    all_x = torch.FloatTensor(np.array(all_x))
    all_y = torch.LongTensor(np.array(all_y))
    all_s = torch.cat(all_s, dim=0)

    # source_valid_x = torch.FloatTensor(np.array(source_valid_x))
    # source_valid_y = torch.LongTensor(np.array(source_valid_y))
    # source_valid_s = torch.cat(source_valid_s, dim=0)
    
    if target_sample>0:
        # source_train_x = temporal_interpolation(source_train_x, target_sample, use_avg=use_avg)
        # source_valid_x = temporal_interpolation(source_valid_x, target_sample, use_avg=use_avg)
        all_x = temporal_interpolation(all_x, target_sample, use_avg=use_avg)
        
    if use_channels is not None:
        # train_dataset = eeg_dataset(source_train_x[:,use_channels,:],source_train_y,source_train_s)
        dataset = eeg_dataset(all_x[:,use_channels,:],all_y,all_s)
    else:
        # train_dataset = eeg_dataset(source_train_x,source_train_y,source_train_s)
        dataset = eeg_dataset(all_x,all_y,all_s)
    
    return dataset







def get_data_ablation(sub,sub_list,data_path,few_shot_number = 1, is_few_EA = False, target_sample=-1, use_avg=True, use_channels=None):
    
    target_session_1_path = os.path.join(data_path,r'sub{}_train/Data.mat'.format(sub))
    target_session_2_path = os.path.join(data_path,r'sub{}_test/Data.mat'.format(sub))

    session_1_data = sio.loadmat(target_session_1_path)
    session_2_data = sio.loadmat(target_session_2_path)
    R = None
    if is_few_EA is True:
        session_1_x = EA(session_1_data['x_data'],R)
    else:
        session_1_x = session_1_data['x_data']
        
    if is_few_EA is True:
        session_2_x = EA(session_2_data['x_data'],R)
    else:
        session_2_x = session_2_data['x_data']
    
    # -- debug for BCIC 2b
    test_x_1 = torch.FloatTensor(session_1_x)      
    test_y_1 = torch.LongTensor(session_1_data['y_data']).reshape(-1)

    test_x_2 = torch.FloatTensor(session_2_x)      
    test_y_2 = torch.LongTensor(session_2_data['y_data']).reshape(-1)
    
    if target_sample>0:
        test_x_1 = temporal_interpolation(test_x_1, target_sample, use_avg=use_avg)
        test_x_2 = temporal_interpolation(test_x_2, target_sample, use_avg=use_avg)

    
    # Target Subject ID (sub)를 데이터 개수만큼 할당하여 텐서 생성
    test_y_concat = torch.cat([test_y_1, test_y_2], dim=0)
    test_s_concat = torch.ones((len(test_y_concat),)) * sub

    if use_channels is not None:
        test_dataset = eeg_dataset(torch.cat([test_x_1,test_x_2],dim=0)[:,use_channels,:],torch.cat([test_y_1,test_y_2],dim=0), test_s_concat)
    else:
        test_dataset = eeg_dataset(torch.cat([test_x_1,test_x_2],dim=0),torch.cat([test_y_1,test_y_2],dim=0), test_s_concat)

    source_train_x = []
    source_train_y = []
    source_train_s = []
    
    source_valid_x = []
    source_valid_y = []
    source_valid_s = []
    subject_id = 0
    for i in range(1, 10):
        if i == sub:
            continue
        if i not in sub_list:
            continue
        train_path = os.path.join(data_path,r'sub{}_train/Data.mat'.format(i))
        train_data = sio.loadmat(train_path)
    
        test_path = os.path.join(data_path,r'sub{}_test/Data.mat'.format(i))
        test_data = sio.loadmat(test_path)
        if is_few_EA is True:
            session_1_x = EA(train_data['x_data'],R)
        else:
            session_1_x = train_data['x_data']

        session_1_y = train_data['y_data'].reshape(-1)

        train_x,valid_x,train_y,valid_y = train_test_split(session_1_x,session_1_y,test_size = 0.1,stratify = session_1_y)
        
        source_train_x.extend(train_x)
        source_train_y.extend(train_y)
        source_train_s.append(torch.ones((len(train_y),))*subject_id)

        source_valid_x.extend(valid_x)
        source_valid_y.extend(valid_y)
        source_valid_s.append(torch.ones((len(valid_y),))*subject_id)

        if is_few_EA is True:
            session_2_x = EA(test_data['x_data'],R)
        else:
            session_2_x = test_data['x_data']

        session_2_y = test_data['y_data'].reshape(-1)

        train_x,valid_x,train_y,valid_y = train_test_split(session_2_x,session_2_y,test_size = 0.1,stratify = session_2_y)
        
        source_train_x.extend(train_x)
        source_train_y.extend(train_y)
        source_train_s.append(torch.ones((len(train_y),))*subject_id)

        source_valid_x.extend(valid_x)
        source_valid_y.extend(valid_y)
        source_valid_s.append(torch.ones((len(valid_y),))*subject_id)
        subject_id+=1
    
    source_train_x = torch.FloatTensor(np.array(source_train_x))
    source_train_y = torch.LongTensor(np.array(source_train_y))
    source_train_s = torch.cat(source_train_s, dim=0)

    source_valid_x = torch.FloatTensor(np.array(source_valid_x))
    source_valid_y = torch.LongTensor(np.array(source_valid_y))
    source_valid_s = torch.cat(source_valid_s, dim=0)
    
    if target_sample>0:
        source_train_x = temporal_interpolation(source_train_x, target_sample, use_avg=use_avg)
        source_valid_x = temporal_interpolation(source_valid_x, target_sample, use_avg=use_avg)
        
    if use_channels is not None:
        train_dataset = eeg_dataset(source_train_x[:,use_channels,:],source_train_y,source_train_s)
    else:
        train_dataset = eeg_dataset(source_train_x,source_train_y,source_train_s)
    
    if use_channels is not None:
        valid_datset = eeg_dataset(source_valid_x[:,use_channels,:],source_valid_y,source_valid_s)
    else:
        valid_datset = eeg_dataset(source_valid_x,source_valid_y,source_valid_s)
    
    return train_dataset,valid_datset,test_dataset







TOTAL_SUBS = list(range(1, 10)) 


def make_sub_loso(total_subs, sub_k):
    plan = {}  # {left_out: [train_sub1, train_sub2, train_sub3]}
    for left_out in total_subs:
        pool = [s for s in total_subs if s != left_out]
        if sub_k > len(pool):
            raise ValueError(f"SUB={sub_k} > available train subjects={len(pool)} (left_out={left_out})")
        chosen = random.sample(pool, sub_k)
        plan[left_out] = chosen
    return plan

SUBLOSO = make_sub_loso(TOTAL_SUBS, ENTIRE_SUB)
print("SUBLOSO:", SUBLOSO)  # 확인용
# exit()


seed_torch(8)



if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
    # entire_dataset = get_data_ablation_entire_data(data_path,1,is_few_EA = True, target_sample=1024)
    train_dataset,valid_dataset,test_dataset = get_data_ablation(SUB,SUBLOSO[SUB],data_path,1,is_few_EA = True, target_sample=1024)
elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
    train_dataset,valid_dataset,test_dataset = get_data_ablation(SUB,SUBLOSO[SUB],data_path,1,is_few_EA = True, target_sample=1024)


# print(f"Subject {1} - Train: {len(train_dataset)}, Valid: {len(valid_dataset)}, Test: {len(test_dataset)}")
# exit()


print("-" * 100)



batch_size=64


if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
    if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
        target_loader = torch.utils.data.DataLoader(train_dataset, batch_size=batch_size, num_workers=0, shuffle=False)
    elif TRAIN_OR_VALID_OR_TEST == 'VALID':
        target_loader = torch.utils.data.DataLoader(valid_dataset, batch_size=batch_size, num_workers=0, shuffle=False)
    elif TRAIN_OR_VALID_OR_TEST == 'TEST':
        target_loader = torch.utils.data.DataLoader(test_dataset, batch_size=batch_size, num_workers=0, shuffle=False)
    # target_loader = torch.utils.data.DataLoader(entire_dataset, batch_size=batch_size, num_workers=0, shuffle=False)
elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
    target_loader = torch.utils.data.DataLoader(test_dataset, batch_size=batch_size, num_workers=0, shuffle=False)


print("*" * 100)
# print(f"SUB {i} START")
print(f"NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL = {NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL}")

if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'NOTRAIN':
    model = LitEEGPTCausal(load_path=f'/workspace/EEGPT/EEGPT-main/checkpoint/eegpt_mcae_58chs_4s_large4E.ckpt')
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'LINEAR':
    model = LitEEGPTCausal(load_path=f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_4_LP_NO_CHAN_CONV_csv/subject1/epoch=82_step=5395_best_checkpoint.ckpt')
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'FINETUNE':
    model = LitEEGPTCausal(load_path=f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_2_FT_csv/subject1/epoch=6_step=455_best_checkpoint.ckpt')
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'PEARL':
    model = LitEEGPTCausal(load_path=f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_5_PEARL_csv/subject1/epoch=48_best_checkpoint.ckpt')
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DYNAMICPEARL':
    model = LitEEGPTCausal(load_path=f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_11_PEARL_MULTI_SCALE_DYNAMIC_CONV_KERNEL_6,11,16,21,26,31,36,41_REDUCTION_2_csv/subject1/epoch=74_best_checkpoint.ckpt')
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'LORA':
    model = LitEEGPTCausal()
    model = apply_lora_to_eeg_encoder(model)
    ckpt_path = f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_3_LORA_only_qkv_RANK_8_NO_CHAN_CONV_csv/subject1/epoch=18_best_checkpoint.ckpt'
    checkpoint = torch.load(ckpt_path, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=True)
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DORA':
    model = LitEEGPTCausal()
    model = apply_lora_to_eeg_encoder(model)
    ckpt_path = f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_1_DORA_only_qkv_RANK_8_NO_CHAN_CONV_csv/subject1/epoch=17_best_checkpoint.ckpt'
    checkpoint = torch.load(ckpt_path, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=True)
elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'VERA':
    model = LitEEGPTCausal()
    model = apply_vera_to_eeg_encoder(model)
    ckpt_path = f'/workspace/EEGPT/log/BCIC2A_log/EEGPT/260727_8_VERA_only_qkv_RANK_256_NO_CHAN_CONV_csv/subject1/epoch=74_best_checkpoint.ckpt'
    checkpoint = torch.load(ckpt_path, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=True)
else:
    print("ENTER NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL CORRECTLY")
    exit()

print(f"DEVICE = {device}")
model.to(device)
model.eval()
# exit()


layer_feature_lists = [[] for _ in range(8)]
y_list = []  # Label
s_list = []  # Subject id

with torch.no_grad():
    for step, data in enumerate(target_loader):
        if len(data) != 3:
            raise ValueError(f"[Error] DataLoader에서 정확히 3개의 요소(inputs, labels, subject_ids)가 반환되지 않았습니다. 현재 요소 개수: {len(data)}")
        
        inputs, labels, subject_ids = data
        inputs = inputs.to(device)

        features = model(inputs)

        for i in range(8):
            layer_feature_lists[i].append(features[i].detach().cpu().clone())

        y_list.append(labels.detach().cpu().clone())
        s_list.append(subject_ids.detach().cpu().clone())

        # 주기적으로 GPU 및 CPU 캐시를 비워줍니다.
        if step % 4 == 0:
            torch.cuda.empty_cache()
            gc.collect()
    
    if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
        if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
            entire_dataset_length = len(train_dataset)
        elif TRAIN_OR_VALID_OR_TEST == 'VALID':
            entire_dataset_length = len(valid_dataset)
        elif TRAIN_OR_VALID_OR_TEST == 'TEST':
            entire_dataset_length = len(test_dataset)
        # entire_dataset_length = len(entire_dataset)
    elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
        entire_dataset_length = len(test_dataset)
    print(f"entire_dataset_length = {entire_dataset_length}")

    y_tensor = torch.cat(y_list, dim=0)
    s_tensor = torch.cat(s_list, dim=0)

    del y_list
    del s_list
    torch.cuda.empty_cache()
    gc.collect()

    rearranged_features = []


    for layer_num in range(1, 9):
        layer_idx = layer_num - 1
        print(f"Processing and saving Layer {layer_num}...")

        layer_tensor = torch.cat(layer_feature_lists[layer_idx], dim=0)

        layer_feature_lists[layer_idx] = None
        gc.collect()

        print(f"Before rearrange shape = {layer_tensor.shape}")
        rearranged = rearrange(layer_tensor, '(B N) dim1 dim2 -> B N dim1 dim2', B=entire_dataset_length)
        print(f"After rearrange shape = {rearranged.shape}")

        # dim1 축(mC + embed_num 토큰)에서 마지막 embed_num개, 즉 summary_token 부분만 남긴다.
        # forward()에서 실제 classifier(linear_probe1/2)로 전달되는 것도 이 summary_token뿐이므로,
        # raw 채널 패치 토큰을 함께 두고 평균 내면 실제 쓰이지 않는 정보가 섞여 들어간다.
        rearranged = rearranged[:, :, -EMBED_NUM:, :]
        print(f"After summary_token slicing shape = {rearranged.shape}")

        if len(POOLING) != 0:
            rearranged = rearranged.mean(dim=POOLING)
            print(f"After pooling shape = {rearranged.shape}")

        del layer_tensor
        torch.cuda.empty_cache()
        gc.collect()




        if NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'NOTRAIN':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/NOTRAIN_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/NOTRAIN_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/NOTRAIN_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/NOTRAIN_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'LINEAR':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/linear_probe_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/linear_probe_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/linear_probe_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/linear_probe_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'FINETUNE':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/finetune_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/finetune_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/finetune_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/finetune_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'PEARL':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/pearl_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/pearl_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/pearl_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/pearl_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DYNAMICPEARL':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/dynamic_pearl_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/dynamic_pearl_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/dynamic_pearl_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/dynamic_pearl_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'LORA':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/LORA_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/LORA_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/LORA_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/LORA_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'DORA':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/DORA_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/DORA_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/DORA_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/DORA_model/SUB{SUB}_Model/layer_{layer_num}"
        elif NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL == 'VERA':
            if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
                # save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/entire/VERA_model/SUB{SUB}_Model/layer_{layer_num}"
                if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/train/VERA_model/SUB{SUB}_Model/layer_{layer_num}"
                elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                                    save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/valid/VERA_model/SUB{SUB}_Model/layer_{layer_num}"
            elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
                save_dir = f"/workspace/EEGPT/dataset/BCIC-2A/layer_wise/only_test/VERA_model/SUB{SUB}_Model/layer_{layer_num}"
        else:
            print("WRITE LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA CORRECTLY")
            exit()
        os.makedirs(save_dir, exist_ok=True)

        save_dict = {
        "x": rearranged,
        "y": y_tensor,
        "s": s_tensor}


        if ENTIRE_OR_ONLY_TEST == 'ENTIRE':
            if TRAIN_OR_VALID_OR_TEST == 'TRAIN':
                save_path = os.path.join(save_dir, "train_features_labels_subjectids.pt")
            elif TRAIN_OR_VALID_OR_TEST == 'VALID':
                            save_path = os.path.join(save_dir, "valid_features_labels_subjectids.pt")
        elif ENTIRE_OR_ONLY_TEST == 'ONLY_TEST':
            save_path = os.path.join(save_dir, "test_features_labels_subjectids.pt")
        torch.save(save_dict, save_path)

        del rearranged, save_dict
        torch.cuda.empty_cache()
        gc.collect()




print(f"BCIC2A {NOTRAIN_LINEAR_OR_FINETUNE_OR_PEARL_LORA_OR_DORA_VERA_OR_DYNAMICPEARL} {ENTIRE_OR_ONLY_TEST} {TRAIN_OR_VALID_OR_TEST} FEATURE EXTRACTION END")