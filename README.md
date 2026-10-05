# Zalo10Headless
Zalo10's headless service: keeps the Zalo WebSocket running even when the app UI is completely closed so you can still receive messages
and push them into BlackBerry Hub. Reuses `ZaloService` from the `Zalo10` project (`../Zalo10/src`),
so the two projects need to be next to each other. When the UI is open, the service hands over the WebSocket (see `src/ServiceHandoff.hpp` in the Zalo10 project). Packaged together into Zalo10's .bar (see Zalo10's bar-descriptor.xml).
