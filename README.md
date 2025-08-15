
<div align="center">
  <img alt="JetKVM Logo" src="https://jetkvm.com/logo-blue.png" height="40">

**jetkvm-native**

[Website](https://jetkvm.com) • [Issues](https://github.com/jetkvm/jetkvm-native/issues) • [Docs](https://jetkvm.com/docs) • [Discord](https://jetkvm.com/discord)

</div>

JetKVM native is a part of JetKVM application, but written in C and needs to be compiled with Rockchip SDK. It mainly provides the following functions:

- HDMI capture
- Touchscreen

---

## Quick Start

#### 1. Install dependencies

```bash
sudo apt-get update &&
sudo apt-get install -y --no-install-recommends \
  build-essential \
  device-tree-compiler \
  gperf g++-multilib gcc-multilib \
  libnl-3-dev libdbus-1-dev libelf-dev libmpc-dev dwarves \
  bc openssl flex bison libssl-dev python3 python-is-python3 texinfo kmod cmake
```

#### 2. Download the latest SDK

```bash
BUILDKIT_VERSION="v0.2.5"
wget https://github.com/jetkvm/rv1106-system/releases/download/${BUILDKIT_VERSION}/buildkit.tar.zst
mkdir -p /opt/jetkvm-native-buildkit
tar --use-compress-program="unzstd --long=31" -xvf buildkit.tar.zst -C /opt/jetkvm-native-buildkit
```

#### 3. Compilation

```bash
make
```

## Contributing

We welcome contributions from the community! Whether it's improving the firmware, adding new features, or enhancing documentation, your input is valuable. We also have some rules and taboos here, so please read this page and our [Code of Conduct](/CODE_OF_CONDUCT.md) carefully.

Please also review our Code of Conduct to keep the community welcoming and constructive.
