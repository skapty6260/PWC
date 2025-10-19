Чистый wayland. По крайней мере, рендер.
С инпутом похуй.
Делаю тестовый рендер для 3д, тестовый для 2д (Клиенты/приложения/попапы)
Сразу начинаю делать приложение настроек, там контролирую сцены графически

https://support.tools/advanced-linux-graphics-drm-programming/
https://youtu.be/WNRQLessSms
https://www.youtube.com/watch?v=g7Jlyk4Xp4o&list=PLA0dXqQjCx0RntJy1pqje9uHRF1Z5vZgA&index=3

TODO:
Делаю нормальные callback message для всех валидационных приколов.
Делаю рефакторинг вулкана и демо.
Делаю структуру рендера со сценами, функциями для запуска дисплея, cleanup и тд.
Делаю запуск рендера, с другим тестовым самым простым демо (Можно тупо черный дисплей).
Делаю дерево сцены (Root->workspaces->containers
                                    ->background)
Делаю инпут для переключения между workspaces для проверки вывода прототипов контейнеров разных цветов.
Делаю методы рендера для кадров, damage, transactions
Дальше реализую wayland server и dbus ipc 