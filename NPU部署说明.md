# V1.3 NPU部署说明

## 编译入口

在Qt Creator中打开：

`srcs/mainbasic/mainbasic_DL_merge_test_rk3568.pro`

工程继续链接OpenCV 3.4.12，并额外链接项目内的RKNN Runtime 2.3.2。

## 板端必需文件

1. 将`lib/rknn_runtime_2.3.2/arm64/librknnrt.so`部署到：

   `/root/screener-libs/librknnrt.so`

2. 将已经验证的`c800_fp16.rknn`部署到：

   `/root/screener-models/pupil/releases/spatial_concat_800x160_dataset12/c800_fp16.rknn`

3. 保留CPU回退模型：

   `/root/screener-models/pupil/releases/spatial_concat_800x160_dataset12/model_fp32.onnx`

`librknnrt.so`是程序启动依赖，必须先部署；RKNN模型加载或推理失败时，算法会自动改用ONNX CPU后端。

## 启动后检查

NPU初始化成功时日志应包含：

```text
[DL_INIT] model=c800,backend=rknn_npu,status=ready
```

CPU回退模型加载成功时日志应包含：

```text
[DL_INIT] model=c800,backend=onnx_cpu,status=ready
```

如果NPU运行期异常，日志会输出`disable_npu_and_fallback_cpu`，本次程序运行期间不再重复调用失败的NPU上下文，重启程序后重新尝试NPU。
