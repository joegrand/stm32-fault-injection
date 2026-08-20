# STM32F2 / F4 Fault Injection

Authors: [Joe Grand](https://joegrand.com) and [Lennert Wouters](https://x.com/lennertwo)

The [STMicroelectronics](https://www.st.com/) STM32 family of microcontrollers is deployed in billions of embedded systems, making them desirable, high-value targets. In particular, the [STM32F2](https://www.st.com/en/microcontrollers-microprocessors/stm32f2-series.html) and [STM32F4](https://www.st.com/en/microcontrollers-microprocessors/stm32f4-series.html) have been heavily scrutinized due to their use in popular cryptocurrency hardware wallets like the [Trezor One](https://trezor.io/trezor-model-one), [Trezor Model T](https://trezor.io/trezor-model-t), and [KeepKey](https://keepkey.com/). Previous research has shown that fault injection can bypass protection mechanisms and enable flash memory extraction. However, those techniques can lead to device corruption or permanent loss of data.

Our work comprises three years of refining and extending these attacks into a more repeatable and reliable process for extracting protected flash memory from STM32F2 and STM32F4 devices, including:

* Characterizing fault injection parameters using EMFI (Electromagnetic Fault Injection) and VFI (Voltage Fault Injection)
* Downgrading from STM32 RDP level 2 to level 1 using EMFI and VFI
    * We found that the STM32F2 is susceptible to a very early downgrade using EMFI
* Bootloader command glitching to safely extract flash memory contents
* A new strategy that combines two known attacks: Our RDP level 2 to level 1 downgrade and the [Flash Patch and Breakpoint (FPB)](#flash-patch-and-breakpoint-fpb) attack
    * This allows us to extract the full flash contents using only two glitches

This repository accompanies our [REcon 2026](https://recon.cx/2026/en/index.html) presentation "[Failure Is Not an Option: A Reliable Process to Exploit STM32F2/F4 Microcontrollers](https://www.youtube.com/watch?v=eAb7yu5bBro)" and contains the following:

* [Jupyter notebooks](https://github.com/joegrand/stm32-fault-injection/tree/master/notebooks) and associated wrappers for the ChipWhisperer and STM32 bootloader
* [Source code](https://github.com/joegrand/stm32-fault-injection/tree/master/source) for characterization test code and FPB payload
* [Hardware designs](https://github.com/joegrand/stm32-fault-injection/tree/master/hardware) for evaluation and attack
* [Slides](https://github.com/joegrand/stm32-fault-injection/tree/master/slides)

This project is unsupported, but for public discussion you can submit an [Issue](https://github.com/joegrand/stm32-fault-injection/issues) or visit [Joe Grand's Discord server](https://discord.com/invite/wud8KnF2Gm).

## Requirements

### Hardware

* [ChipWhisperer-Husky](https://www.newae.com/products/nae-cwhusky)
* [NewAE CW308 UFO Board](https://rtfm.newae.com/Targets/CW308%20UFO/)
* [Segger J-Link](https://www.segger.com/products/debug-probes/j-link/)
* [ChipSHOUTER](https://www.newae.com/products/nae-cw520) (Optional)
* CNC Milling Machine w/ [GRBL](https://github.com/gnea/grbl) controller (for example, [3018 Pro-MAX](https://www.amazon.com/LUNYEE-MAX-All-Metal-Switches-Emergency-Stop/dp/B0C6WQWX3R) w/ [Doesbot](https://www.amazon.com/gp/product/B0C625ZCN7/)) (Optional)
* Target device: [STM32F2](https://www.st.com/en/microcontrollers-microprocessors/stm32f2-series.html) or [STM32F4](https://www.st.com/en/microcontrollers-microprocessors/stm32f4-series.html) with socket/adapter board


### Software

To run the code, you will need Python 3 and some additional packages. Assuming you have Python 3 installed, you should be able to create a new virtual environment using the following instructions:

* Clone this repository: `git clone https://github.com/joegrand/stm32-fault-injection`
* Create a virtual environment: `cd stm32-fault-injection && python3 -m venv stm32_venv`
* Activate the virtual environment: `source ./stm32_venv/bin/activate`
* Update pip: `python -m pip install --upgrade pip`
* Install the required libraries: `python -m pip install -r requirements.txt`
* Install from source:
  * PyLink must include the [Add raw JTAG support #86](https://github.com/square/pylink/pull/86) pull request. This is not currently in their main repository, so it should be installed from source with this PR added locally.
  * You may want to [install the ChipWhisperer software from source](https://chipwhisperer.readthedocs.io/en/latest/installation.html) to get the latest version. This code has been developed and tested with v6.0.0.
* Start the Jupyter server: `cd notebooks && python -m jupyter notebook`


## Other Resources

### RDP Downgrade

* [wallet.fail](https://media.ccc.de/v/35c3-9563-wallet_fail) and [chip.fail](https://www.youtube.com/watch?v=CX71p_qcCxY)
* [Kraken Identifies Critical Flaw in Trezor Hardware Wallets](https://blog.kraken.com/post/3662/kraken-identifies-critical-flaw-in-trezor-hardware-wallets/)
* [Inside Kraken Security Labs: Flaw Found in Keepkey Crypto Hardware Wallet Part 1](https://blog.kraken.com/product/security/flaw-found-in-keepkey-crypto-hardware-wallet) and [Part 2](https://blog.kraken.com/product/security/flaw-found-in-keepkey-crypto-hardware-wallet-part-2)
* [Verifying Code Readout Protection Claims](https://circuitcellar.com/archive-article/verifying-code-readout-protection-claims/)
* [Shedding too much Light on a Microcontroller’s Firmware Protection](https://www.aisec.fraunhofer.de/en/FirmwareProtection.html)
* [SiliconToaster: A Cheap and Programmable EM Injector for Extracting Secrets](https://eprint.iacr.org/2020/1115.pdf)
* [Unlock the Door to my Secrets, but don’t Forget to Glitch](https://tches.iacr.org/index.php/TCHES/article/view/11422)
* [VoidStar Security FI Resources](https://github.com/voidstarsec/fi-resources/)
* [Microcontroller Exploits (Travis Goodspeed)](https://nostarch.com/microcontroller-exploits)

### Bootloader Read Memory Glitch

* [Shaping the Glitch: Optimizing Voltage Fault Injection Attacks](https://tches.iacr.org/index.php/TCHES/article/view/7390)
* [Read secure firmware from STM32F1xx flash using ChipWhisperer (Prog.World)](https://web.archive.org/web/20201021153452/https:/prog.world/read-secure-firmware-from-stm32f1xx-flash-using-chipwhisperer/)
* [Replicant (Wrongbaud)](https://github.com/wrongbaud/replicant)

### Flash Patch and Breakpoint (FPB)
* [One Exploit to Rule them All? On the Security of Drop-in Replacement and Counterfeit Microcontrollers](https://www.usenix.org/system/files/woot20-paper-obermaier.pdf)
* [STM32F1 Pico Pwner (CTXz)](https://github.com/CTXz/stm32f1-picopwner)
* [STM32F4 readout protection exploit (lolwheel)](https://github.com/lolwheel/stm32f4-rdp-workaround)
