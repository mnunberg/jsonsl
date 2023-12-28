# ClusterFuzzLite set up
This folder contains a fuzzing set for [ClusterFuzzLite](https://google.github.io/clusterfuzzlite).

## Reproduce locally

If you'd like to test this the way ClusterFuzzLite runs it (by way of [OSS-Fuzz](https://github.com/google/oss-fuzz)) you can use the steps:

```sh
git clone https://github.com/google/oss-fuzz
git clone https://github.com/mnunberg/jsonsl jsonsl
cd jsonsl

# Build the fuzzers in .clusterfuzzlite
python3 ../oss-fuzz/infra/helper.py build_fuzzers --external $PWD

# Run the fuzzer for 180 seconds
python3 ../oss-fuzz/infra/helper.py run_fuzzer --external $PWD parse_fuzzer -- -max_total_time=180
```