# Running Setup

This repository contains the modified versions of AFLNET, SNPSFuzzer, ResolverFuzz and SAECRED. To run each of them, simply follow the fuzzer instructions in each of the respective folders. ResolverFuzz contains a script `run_campaigns.sh` that runs ResolverFuzz in Forward-only mode. The SAECRED setup, however, is a bit more involved as it requires multiple virtual interface setups; the details are in the next section. **CFS does not require any external modifications to the fuzzers and the hooks are already in place. Simply running each fuzzer by following their tutorials will run CFS alongside the fuzzer.**

# SNPSFuzzer protocol implementation setup

Since SNPSFuzzer does not, by default, give an end-to-end implementation build script, we devised bash scripts for each of the implementations we tested. `./dnsmasq_fuzzing.sh` clones dnsmasq and builds the target, `tinydtls_fuzzing.sh` clones tinyDTLS and builds it. For LightFTP, enter `LightFTP/Source/Release/` and simply `make`.

## Output

Each run will output the fuzzer output along with three additional files: `monitor.log`, `monitor_violations.txt` and `runtime_monitor.txt` where `runtime_monitor.txt` contains `\n` separated violations per test trace. Each `\n` has the following structure `Property ID: <trace>` where the property ID corresponds to the properties violated, and the triggering test case. The exact `ID: Property` mapping can be found in `monitor.log`.

## Note

We understand that fuzzing setups may be non-trivial; unfortunately, that is out of our control, as we are bound by the fuzzer frameworks. At a high level, AFLNET works within Docker containers with the runtime files generated within the specific docker instance. SNPSFuzzer runs natively, and therefore the files will be within the respective `<protocol_implementation_name>_workspace` folders. For ResolverFuzz, the generated files should be in the ResolverFuzz root directory, with additional logs in the `monitor_logs/` directory.

# ResolverFuzz campaign run

After the required ResolverFuzz tutorial is complete, the `run_campaigns.sh` bash script will run ResolverFuzz in the Forward-only mode as reported in the paper. This takes two arguments with flags: `bash run_campaigns.sh --units <number_of_runs> --payload <number_of_test_cases>`. We recommend using units around 1-5 and payloads 5-10 for easy reproduction purposes.


# SAECRED Setup
To run CFS for WPA3, you must compile `hostapd` by following this tutorial:

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
