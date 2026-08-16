# Screeni — Control Flow & Concurrency

## Tracker thread lifecycle
```
start(): exchange(running) -> store.open -> CreateEvent -> thread{thread_main}
  wait ready_event(15s) | setup_ok_
thread_main(): publish tid -> PeekMessage (create queue) -> setup_message_loop
  (RegisterClass, HWND_MESSAGE, SetWinEventHook, SetTimer 1s) -> SetEvent(ready)
  -> on_foreground(GetForegroundWindow) -> GetMessage loop
stop(): stop_requested_ -> PostThreadMessage(WM_QUIT) -> join -> lock(state) flush_current(true)
  -> store.close
```
- win_event_proc (out-of-context, other processes) -> instance_->on_foreground -> lock -> reconcile(force)
- WM_TIMER -> poll_idle_and_flush -> lock -> reconcile(no force)
- reconcile: idle-edge or force -> flush(true) + begin_segment; else 5s flush; else hwnd catch-up.
- Locking: state_mutex_ held across Store calls; Store has own mutex. No deadlock (strict
  ordering: state_mutex_ -> store mutex, never reverse). UI never takes state_mutex_.
- Atomics: running_, stop_requested_, setup_ok_, idle_threshold_sec_, thread_id_, instance_.
  Instance pointer published before hook registration (thread-safe via acquire/release).

## UI control flow
- 15s refreshTimer_ always active (even when window hidden) — unnecessary work.
- changeEvent(WindowStateChange) triggers refreshAll on every restore.
- Update flow: check(1.5s after launch) -> reply -> parse -> cache save -> applyCache ->
  bubble; dismiss caches version; download -> progress % -> installer (detached, silent).
- Timeouts: check 20s (QPointer-guarded abort), download 30min.

## Error handling summary
- Best-effort everywhere in Store (rcs ignored after open), guard clauses for null db/stmts.
- Tracker setup failure -> start() returns false -> main shows critical box.
- Update failures set lastError_ + status; never crash (QPointer guard on reply).

## Risks
- Tracker start timeout path double-closes store? (store.close called in stop() after join —
  close() is idempotent; close_unlocked handles null).
- flush_current uses wall+steady time deltas — monotonic-safe.
- No race found in review; focus-policy change is UI-only.