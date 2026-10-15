cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DWOWDECOMP_DISABLE_SIGNATURE_CHECKS=ON
cmake --build build --config Release --target WowClient --parallel