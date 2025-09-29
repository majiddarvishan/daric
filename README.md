# daric

## Build

- only Logger and ThreadPool

```bash
cmake -S . -B build -DBUILD_LOGGER=ON -DBUILD_THREADPOOL=ON -DBUILD_NETWORKING=OFF
cmake --build build
```

- build everything (default)

```bash
cmake -S . -B build
cmake --build build
```
