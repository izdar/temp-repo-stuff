To run CFS, you must compile `hostapd` by following this tutorial:

## Using simulated Wi-Fi interfaces

On Linux you can create software [simulated Wi-Fi interfaces](https://www.kernel.org/doc/html/latest/networking/mac80211_hwsim/mac80211_hwsim.html)
to more easily and reliably perform certain Wi-Fi experiments.
You can create simulated Wi-Fi interfaces with the following command:

	modprobe mac80211_hwsim radios=4

This will create 4 simulated Wi-Fi interfaces.
Here `mac80211_hwsim` represents a kernel module that will simulate the Wi-Fi interfaces.
The `modprobe` command is used to load this kernel module.
The above command will only work if your Linux distribution (by default) provides the `mac80211_hwsim` kernel module.
See [backport drivers](#id-backport-drivers) to manually compile this kernel module.

Then from the root of the repository:

```bash
cd hostap-wpa3/hostapd/
cp defconfig .config
make clean
make -j 4
bash run_ap.sh
```

This will get the virtual interfaces setup in monitor mode for WiFi testing.

## Download cvc5 1.3.0 and add to $PATH

See the 1.3.0 releases here: https://github.com/cvc5/cvc5/releases/tag/cvc5-1.3.0, download
`<your-os>-static.zip`, unzip it, and add the binary to your path as follows.
```bash
export PATH="<path_to_unzipped_cvc5_folder/bin>:$PATH"
```

## Run hostapd

```bash
sudo ./hostapd -dd -K hostapd_wpa3.conf
```

This will spawn a `hostapd` process. You may now return to the root of the repository.

## Run SAECRED

Enter `WiFiPacketGen-saerm` and follow the opam and dune installation steps. Once complete run:

```bash
dune exec sbf -- --saecred
```

In the event of missing dependencies, install the `opam` dependencies using:

```bash
opam install <library>
```

Rerun the `dune` command.

## Run the WiFi driver and LTL-runtime monitor

Enter `wifi-driver-saerm/src`, and run:

```bash
./run_fuzzer
```

Install missing dependencies using:

```bash
sudo apt install <dependency>
```

This will spawn the driver and the C++ runtime monitor. Results of the monitor will populate in `wifi-driver-saerm/src/runtime_monitor.txt`. Once you are satisfied by the campaign time, simply `CTRL+C` and run the `wifi-driver-saerm/src/runtime_filter.py` to filter out and parse the runtime monitor results.
