# Zalo10Headless
Service headless của Zalo10: giữ WebSocket Zalo khi app UI đã đóng hẳn để vẫn nhận tin
và đẩy vào BlackBerry Hub. Dùng lại `ZaloService` của project `Zalo10` (`../Zalo10/src`),
nên hai project phải nằm cạnh nhau. UI mở thì service nhường WebSocket (xem `src/ServiceHandoff.hpp`
trong project Zalo10). Đóng gói chung vào .bar của Zalo10 (xem bar-descriptor.xml của Zalo10).
