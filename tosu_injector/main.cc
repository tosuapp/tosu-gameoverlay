#include <Windows.h>
#include <filesystem>

int main(int argc, char** argv) {
  UNREFERENCED_PARAMETER(argc);

  if (argc < 2) {
    return 1;
  }

  const auto pid = std::stoi(argv[1]);
  const auto process = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

  const auto library_path =
      std::filesystem::current_path() / "tosu_overlay.dll";
  const auto library_name_addr =
      VirtualAllocEx(process, nullptr, library_path.string().size(),
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

  if (!library_name_addr) {
    printf("failed to allocate memory in target process\n");

    return 2;
  }

  if (!WriteProcessMemory(process, library_name_addr,
                          library_path.string().c_str(),
                          library_path.string().size(), NULL)) {
    printf("failed to write memory in target process\n");

    VirtualFreeEx(process, library_name_addr, library_path.string().size(),
                  MEM_RELEASE);

    return 3;
  }

  if (!CreateRemoteThread(
          process, nullptr, NULL,
          reinterpret_cast<PTHREAD_START_ROUTINE>(
              GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA")),
          library_name_addr, NULL, NULL)) {
    printf("failed to create remote thread in target process\n");

    VirtualFreeEx(process, library_name_addr, library_path.string().size(),
                  MEM_RELEASE);

    return 4;
  }

  return 0;
}