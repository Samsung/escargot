https://github.com/zhanhb/int128/

# int128
int128 with divide/stream support, require C++ 11

```cpp
uint128_t a = 2'000'000'000'000'000'000'000_U128; // single quotes requires C++ 14
int128_t b = 1'000'000'000'000'000'000'000_L128;
cout << setw(40) << setfill('0') << hex << uppercase << a / b << endl;
setlocale(LC_ALL, "zh_CN.UTF-8");
cout.imbue(locale());
cout << a / b << endl;
```