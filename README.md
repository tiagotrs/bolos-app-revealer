# BOLOS app for Nano S using visual cryptography for seed backup

Nano S application to generate a seed backup encrypted for a Revealer Card.
For information about Revealer, please visit [revealer] 

/!\ Developpment in progress, use at your own risk

# Setting up dev environnement
Please follow [instructions]

# Making/loading the app
Plug your Nano S, unlock it, go to dashboard
```sh
$ cd /bolos-app-revealer 
$ make all load
```

# Generating the encrypted seed backup
- Open the app on your Nano S
- Navigate to "Type your noise seed" menu
- Type your noise seed (noise seed is the number provided with your revealer card), use left/right button to switch digits, and both buttons to validate digit.
The app will let you know if the code is valid, by checking the included checksum. If it is valid, the app will generate the noise image, the process is quite long (~3min).
- Navigate to "Type your seedphrase" menu, choose the number of words, type your words. The app will check the typed words against the masterseed from your ledger.

Once both noise seed and words are set, the device will display 'Encrypted backup ready' and you can launch python revealer script
```sh
$ python revealer.py --apdu
```

[revealer]: <https://revealer.cc/>
[instructions]: <https://ledger.readthedocs.io/en/latest/userspace/getting_started.html>
