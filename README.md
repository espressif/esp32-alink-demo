本工程为对接阿里云平台，用户无需关心具体的交互流程，只需调用几个简单的接口便能完成与阿里云的数据交互。

# Feature:
1. 配网(一键配网、softap配网)
2. 绑定、解绑
3. OTA升级
4. 透传、非透传(由于esp32的性能十分强劲，建议使用非透传的模式)

# 文件结构
    esp-alink
    ├── bin                                     // 存放所有生成的bin文件
    ├── build                                   // 存放所有生成的.o,.d文件
    ├── burn.sh                                 // ubuntu下烧录脚本
    ├── components
    │   ├── alink
    │   │   ├── component.mk
    │   │   ├── include
    │   │   │   ├── app
    │   │   │   │   ├── alink_export.h         // 用户使用的接口描述
    │   │   │   │   ├── alink_export_rawdata.h // 用户使用的接口描述
    │   │   │   │   ├── alink_json.h
    │   │   │   │   ├── alink_user_config.h    // 用户使用的接口描述
    │   │   │   │   ├── aws_smartconfig.h
    │   │   │   │   └── aws_softap.h
    │   │   │   ├── platform
    │   │   │   └── product
    │   │   ├── lib
    │   │   └── src
    │   │       ├── app
    │   │       │   ├── alink_key.c             // 出厂设置按键实现
    │   │       │   ├── alink_main.c            // alink程序入口
    │   │       │   ├── aws_smartconfig.c       // 一键配网
    │   │       │   ├── aws_softap.c            // softap配网
    │   │       │   ├── sample.c                // alink透传模式的封装
    │   │       │   └── sample_json.c           // alink非透传模式的封装
    │   │       ├── platform
    │   │       └── product
    │   └── test
    │       ├── log.c
    │       ├── tmp.c
    │       ├── tree.md
    │       └── udp_server_test.c
    ├── esp-idf                                 // esp32 SDK
    ├── gen_misch.sh                            // 编译脚本
    ├── main
    │   ├── app_main.c                          // 一个简单的alink sample 程序
    │   └── component.mk
    ├── Makefile
    ├── path_config.sh                          // SDK路径配置脚本
    ├── README.md
    ├── sdkconfig
    └── sdkconfig.old

# 知识储备
1. 熟悉esp32_idf的使用，详见：http://esp-idf.readthedocs.io/en/latest/?badge=latest
2. 了解阿里智能相关知识，详见：https://open.aliplus.com/docs/open

# 硬件条件
  * esp32模组
  * led灯

# 配置与编译
1. 使用'make menuconfig'中的alink_config来配置alink的工作模式、任务优先级
2. 本工程提供相关的编译下载脚本（gen_mish.sh:编译，burn.sh:编译及下载，path_config.sh:路径配置), 如果你想要详细了解esp32相关配置详见esp-idf/README.md

# 运行与调试
1. 下载阿里[智能厂测包](https://open.aliplus.com/download?spm=0.0.0.0.J4tDWU)，
2. 登陆淘宝账号
3. 添加设备：按分类查找 > 模组认证 > 阿里直连配网V2(透传模式选择名称带LUA标识的)
4. 设备网络：密码需手动输入
5. 出厂设置：给GPIO0一高脉冲(如若使用开发板，单击boot按键即可)

# 注意事项
* 设备开机第一次要完整上报所有状态信息. 之后的状态发生改变或者服务器下行命令时才上报数据.
* 设备上报数据需要做去重复,减缓服务器压力,建议两包数据的间隔不低于500ms
* 前期开发可以用沙箱环境开发,项目稳定后切换到线上环境，在sample.c 默认的alink参数，仅供前期调试使用，真实产品开发需向阿里申请厂家的设备接入参数
* alink常用接口均在esp_alink.h中


# 获取工程
要获取lenovo音频项目，请使用以下命令：
git clone --recursive https://gitlab.espressif.cn:6688/customer/esp-alink.git

# Related links
* ESP32概览 : http://www.espressif.com/en/products/hardware/esp32/overview
* ESP32资料 : http://www.espressif.com/en/products/hardware/esp32/resources
* 烧录工具  : http://espressif.com/en/support/download/other-tools
* 串口驱动  : http://www.silabs.com/products/mcu/Pages/USBtoUARTBridgeVCPDrivers.aspx
* 阿里智能开放平台：https://open.aliplus.com/docs/open/
