# spike-bender

This tool is desined for flattering the dynamics of the audio signal.

The overall behaviour is the following:
* Apply frequency weighting function to the input file. Available weighting
  functions are the following:
    * no weighting;
    * A-weighted (IEC 61672-1:2013);
    * B-weighted (IEC 61672-1:2013);
    * C-weighted (IEC 61672-1:2013);
    * D-weighted (IEC 61672-1:2013);
    * K-weighted (ITU-R BS.1770-4/2015).
* Determine the maximum RMS values of the frequency-weighted signal.
* Perform the following  operation for the specified number of passes:
  * Estimate the short-time RMS value which is controlled by the
    `reactivity` parameter.
  * Configure the upward compressor according to the computed RMS value.
  * Adjust the volume of the audio file within the selected dynamics range
    to match the specified RMS value.
* Find the peaks that exceed the median peak value for more than configured threshold and eliminate them.
* Normalize the output file if required.
* Write the output file.

Using The Software
======

## Help

Use the following command to see the software usage:

```bash
spike-bender --help
```

The available option list will be the following:

```
  -dr, --dynamic-range    Dynamic range of the compressor (in dB, 6 dB by default)
  -ep, --eliminate-peaks  The threshold above which all peaks are eliminated (in dB, 1 dB by default, off if not positive),
  -if, --in-file          The path to the input file
  -k, --knee              Knee of the compressor (in dB, 3 dB by default)
  -n, --normalize         Set normalization mode (none, above, below, always, none by default)
  -ng, --norm-gain        Set normalization peak gain (in dB, 0 dB by default)
  -np, --num-passes       Number of passes, 1 by default
  -of, --out-file         The path to the output file
  -r, --reactivity        Reactivity of the compressor (in ms, 40 ms by default)
  -sr, --srate            Sample rate of output (processed) file, optional
  -wf, --weighting        Frequency weighting function (none, a, b, c, d, k, none by default)
```

Requirements
======

The following packages need to be installed for building:

* gcc >= 7.5.0
* GNU make >= 4.2.1
* libsndfile
* libiconv

Building
======

To build the tool, perform the following commands:

```bash
make config # Configure the build
make fetch # Fetch dependencies from Git repository
make
sudo make install
```

To get more build options, run:

```bash
make help
```

To uninstall library, simply issue:

```bash
make uninstall
```

To clean all binary files, run:

```bash
make clean
```

To clean the whole project tree including configuration files, run:

```bash
make prune
```

To fetch all possible dependencies and make the source code tree portable between
different architectures and platforms, run:

```bash
make tree
```

To build source code archive with all possible dependencies, run:

```bash
make distsrc
```