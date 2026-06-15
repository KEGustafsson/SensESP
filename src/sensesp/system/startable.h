#ifndef SENSESP_SYSTEM_STARTABLE_H
#define SENSESP_SYSTEM_STARTABLE_H

namespace sensesp {

/**
 * @brief No-op Startable shim kept only for source compatibility.
 *
 * All methods are no-ops: there is no startup ordering or callback dispatch.
 * Startup is now handled by the event loop, so inheritance from this class
 * should be removed.
 */
class [[deprecated(
    "Startable is a no-op kept only for source compatibility; remove "
    "inheritance, startup is handled by the event loop")]] Startable {
 public:
  Startable(int priority = 0) {}
  virtual void start() {}
  const int get_start_priority() { return 0; }
  void set_start_priority(int priority) {}
  static void start_all() {}
};

}  // namespace sensesp

#endif  // SENSESP_SRC_SENSESP_SYSTEM_STARTABLE_H_
