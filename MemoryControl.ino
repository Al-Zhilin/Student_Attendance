class MemoryControl {
  public:
    uint32_t _start_heap = 0;

  public:
    MemoryControl() {
      _start_heap = ESP.getFreeHeap();
    }

    bool check() {
      if (start_heap - ESP.getFreeHeap() < MIN_FREE_HEAP * 1024)  return false;
      return true
    }

    uint32_t getCurrentHeap() {
      return ESP.getFreeHeap();
    }
}