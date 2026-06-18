# CppWiki Server

Этот документ описывает серверный модуль CppWiki — опциональный бэкенд на фреймворке Drogon, который пока реализует каркас API и готовится к будущей коллаборации, синхронизации и интеграции с внешними источниками.

> **Статус:** стадия каркаса (Milestone 7). Все endpoints, кроме `/health`, возвращают заглушки с корректным конвертом ответа.

---

## Содержание

1. [Назначение](#назначение)
2. [Архитектура и границы](#архитектура-и-границы)
3. [Сборка и запуск](#сборка-и-запуск)
4. [Конфигурация](#конфигурация)
5. [Логирование](#логирование)
6. [Маршруты и контракты](#маршруты-и-контракты)
7. [Авторизация и фильтры](#авторизация-и-фильтры)
8. [Контроллеры](#контроллеры)
9. [DTO и формат ответа](#dto-и-формат-ответа)
10. [Дальнейшее развитие](#дальнейшее-развитие)

---

## Назначение

Серверный модуль решает две задачи:

1. **Дать CppWiki уровень сетевого сервиса** — для удалённого доступа, совместного редактирования, блокировок страниц, presence, права доступа и интеграций.
2. **Сохранить оффлайн-первый характер десктопного приложения** — сервер собирается и запускается отдельным бинарником `cppwiki-server` и не связывается ни с Qt Widgets, ни с Qt WebEngine во времени сборки/исполнения.

Десктопное приложение и сервер могут переиспользовать общий код из `src/core/`, `src/document/` и `src/storage/`, но физически это разные цели сборки.

---

## Архитектура и границы

```
src/
  server/
    server_main.cc          # точка входа
    server_application.{h,cc} # инициализация Drogon, запуск цикла
    server_config.h         # статические настройки по умолчанию
    controllers/            # HTTP-контроллеры
      health_controller.{h,cc}
      auth_controller.{h,cc}
      lock_controller.{h,cc}
      presence_controller.{h,cc}
    filters/                # фильтры обработки запросов
      jwt_auth_filter.{h,cc}
    dto/                    # типизованные контракты API
      api_response.h
      health_response.h
      auth_responses.h
      lock_responses.h
      presence_responses.h
    logging/                # конфигурация spdlog
      logger.{h,cc}
  core/        (общий)
  document/    (общий)
  storage/     (общий)
```

### Цели сборки

| Цель | Тип | Описание |
| :--- | :--- | :--- |
| `cppwiki_app` | `qt_add_executable` | Десктопный Qt-клиент; зависимостей от Drogon нет. |
| `cppwiki_server` | `add_executable` | Drogon-бэкенд; не зависит от Qt. |
| `cppwiki_core` | `add_library` (будущее) | UUID, строковые helpers, constants. |
| `cppwiki_document` | `add_library` (будущее) | Модель документа и валидатор. |
| `cppwiki_storage` | `add_library` (будущее) | Репозитории и адаптеры хранилища. |

---

## Сборка и запуск

Бэкенд собирается той же CMake-системой, что и десктопное приложение. В манифесте `vcpkg.json` уже указаны зависимости: `drogon`, `spdlog`, `reflectcpp`.

### Сборка сервера

```bash
# с CMakePresets, если они настроены
# или классическим способом:
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target cppwiki_server
```

### Запуск

```bash
./build/src/server/cppwiki-server
```

После старта сервер слушает адрес и порт, заданные в `ServerConfig` по умолчанию:

```text
0.0.0.0:8080
```

Количество потоков обработки (0 — автовыбор Drogon) и уровень логирования также задаются в `server_config.h`.

### Роуты по умолчанию

```text
GET    /api/v1/health              # без auth
POST   /api/v1/auth/refresh        # требуется Bearer-заголовок
GET    /api/v1/auth/me             # требуется Bearer-заголовок
POST   /api/v1/locks/{pageId}      # требуется Bearer-заголовок
GET    /api/v1/locks/{pageId}      # требуется Bearer-заголовок
DELETE /api/v1/locks/{pageId}      # требуется Bearer-заголовок
POST   /api/v1/presence/{pageId}   # требуется Bearer-заголовок
GET    /api/v1/presence/{pageId}   # требуется Bearer-заголовок
```

---

## Конфигурация

Конфигурация на стадии каркаса статична и описывается структурой `cppwiki::server::ServerConfig`:

```cpp
struct ServerConfig {
  std::string http_host = "0.0.0.0";
  std::uint16_t http_port = 8080;
  std::int32_t thread_num = 0;  // 0 — автовыбор
  std::string log_level = "info";
};
```

В будущем планируется загрузка параметров из JSON-файла или переменных окружения.

---

## Логирование

Для сервера используется `spdlog` через единый shared-логгер:

```cpp
namespace cppwiki::server::logging {

[[nodiscard]] auto ServerLogger() -> std::shared_ptr<spdlog::logger>;
void InitializeLogging();

}
```

- Создаётся цветной stdout-sink.
- Имя логгера: `cppwiki_server`.
- Паттерн: `[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v`.
- Уровень по умолчанию: `info`.
- Инициализация потокобезопасна (`std::call_once`).

---

## Маршруты и контракты

Все ответы пишутся в единый JSON-конверт:

### Успешный ответ

```json
{
  "apiVersion": "v1",
  "ok": true,
  "result": { /* полезная нагрузка */ }
}
```

### Ошибочный ответ

```json
{
  "apiVersion": "v1",
  "ok": false,
  "error": {
    "code": "unauthorized",
    "message": "..."
  }
}
```

Форма конверта идентична контракту, используемому в `editor_bridge` (QWebChannel), что упрощает совместимость существующего фронтенда.

### Таблица маршрутов

| Method | Route | Контроллер | Auth | Назначение |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/v1/health` | `HealthController` | none | Liveness/readiness. |
| `POST` | `/api/v1/auth/refresh` | `AuthController` | Bearer | Заглушка refresh-токена. |
| `GET` | `/api/v1/auth/me` | `AuthController` | Bearer | Заглушка профиля пользователя. |
| `POST` | `/api/v1/locks/{pageId}` | `LockController` | Bearer | Взять / продлить блокировку страницы. |
| `DELETE` | `/api/v1/locks/{pageId}` | `LockController` | Bearer | Освободить блокировку страницы. |
| `GET` | `/api/v1/locks/{pageId}` | `LockController` | Bearer | Показать состояние блокировки. |
| `POST` | `/api/v1/presence/{pageId}` | `PresenceController` | Bearer | Heartbeat presence. |
| `GET` | `/api/v1/presence/{pageId}` | `PresenceController` | Bearer | Список пользователей онлайн. |

---

## Авторизация и фильтры

### JwtAuthFilter

```cpp
namespace cppwiki::server::filters {

class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter> {
  void doFilter(const drogon::HttpRequestPtr& request,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
  // ...
};

}
```

На стадии каркаса фильтр выполняет только ту часть валидации, за которую можно ответственно отвечать сейчас:

- Проверяет наличие заголовка `Authorization`.
- Проверяет, что заголовок начинается с `Bearer `.
- Извлекает токен и прикрепляет его к атрибутам запроса под ключом `jwt_token`.

Если заголовок отсутствует или некорректен, фильтр возвращает `HTTP 401` с error-конвертом:

```json
{
  "ok": false,
  "error": {
    "code": "unauthorized",
    "message": "Missing or invalid Authorization header"
  }
}
```

> **Примечание.** Проверка подписи, источника (`issuer`), аудитории (`audience`) и срока действия (`exp`) — это работа будущего внедрения Authentik (Milestone 6, см. ADR-008).

Контроллеры, которым нужен `user_id`, забирают его из атрибута `jwt_token` — на каркасной стадии это работает как trace-like идентификатор пользователя.

---

## Контроллеры

### HealthController

Проверяет жизнеспособность сервиса. Отвечает `HTTP 200` и короткой нагрузкой:

```json
{
  "apiVersion": "v1",
  "ok": true,
  "result": {
    "status": "ok",
    "version": "0.1.0"
  }
}
```

### AuthController

Используется в заглушечном режиме:

- `POST /api/v1/auth/refresh` — возвращает `access_token` обратно с типом `Bearer` и `expires_in = 3600`.
- `GET /api/v1/auth/me` — возвращает stub-профиль:

```json
{
  "sub": "<token из атрибутов>",
  "email": "user@example.com",
  "display_name": "Stub User",
  "roles": ["wiki.viewer"]
}
```

### LockController

Реализует заготовки API блокировок страниц:

```json
// POST /api/v1/locks/{pageId}
{
  "page_id": "...",
  "acquired": true,
  "released": false,
  "owner_user_id": "...",
  "expires_in_seconds": 30
}

// DELETE /api/v1/locks/{pageId}
{
  "page_id": "...",
  "acquired": false,
  "released": true,
  "owner_user_id": "",
  "expires_in_seconds": 0
}

// GET /api/v1/locks/{pageId}
{
  "page_id": "...",
  "locked": false,
  "owner_user_id": "",
  "acquired_at": "",
  "expires_in_seconds": 0
}
```

### PresenceController

Заготовки presence-механизма совместного просмотра / редактирования:

```json
// POST /api/v1/presence/{pageId}
{
  "page_id": "...",
  "user_id": "...",
  "heartbeat_interval_seconds": 10
}

// GET /api/v1/presence/{pageId}
{
  "page_id": "...",
  "users": []
}
```

---

## DTO и формат ответа

DTO сервера типизированы и сериализуются через `reflect-cpp`. Центральный шаблон:

```cpp
template <typename T>
struct ApiResponse {
  std::string api_version = "v1";
  bool ok = false;
  std::optional<ApiError> error;
  std::optional<T> result;
};
```

Удобные хелперы:

- `dto::SuccessJson(payload)` — оборачивает произвольный DTO `T` в success-конверт.
- `dto::ErrorJson(code, message)` — возвращает error-конверт.

### DTO по группам

| Группа | Структуры | Назначение |
| :--- | :--- | :--- |
| `health_response` | `HealthResponse` | Статус и версия. |
| `auth_responses` | `RefreshResponse`, `UserProfileResponse` | Токены и профиль. |
| `lock_responses` | `LockStateResponse`, `LockActionResponse` | Блокировки страниц. |
| `presence_responses` | `PresentUser`, `PresenceHeartbeatResponse`, `PresenceListResponse` | Пользователи онлайн. |

---

## Дальнейшее развитие

Серверный модуль — это не конечный продукт, а основа. Следующие итерации должны закрыть:

1. **Авторизация через Authentik**
   - JWKS-валидация подписи.
   - Проверка `iss`, `aud`, `exp`.
   - Полноправное отделение `sub` от сырого токена.

2. **Состояние блокировок и presence**
   - Реальные in-memory или persistence-бэкенды (Redis/Couchbase/PostgreSQL).
   - TTL/справедливая экспирация.
   - Conflict-ответ при захвате чужой блокировки.

3. **Переход общих DTO в библиотеки**
   - Выделить `cppwiki_core`, `cppwiki_document`, `cppwiki_storage`, чтобы избежать дублирования между Qt-приложением и бэкендом.

4. **Конфигурация из файлов / env**
   - JSON-конфиг Drogon или переменные окружения для хоста, порта и уровня логирования.

5. **Тесты**
   - Интеграционные тесты контроллеров с `drogon::HttpTest`.
   - Тесты фильтра на отсутствие / некорректный `Authorization`.

6. **Документация API**
   - OpenAPI/Swagger-спецификация по мере стабилизации контрактов.

7. **Связь с хранилищем**
   - Использование `src/storage/` для серверного доступа к документам, когда появится удалённый режим работы.

---

## Связанные документы

- `doc/architecture/adr/ADR-009-server-module.md` — архитектурное решение по модулю сервера.
- `doc/architecture/adr/ADR-008-authentik-auth-model.md` — модель авторизации.
- `doc/roadmap/Current_Roadmap.md` — Roadmap, Milestone 7.
- `src/server/CMakeLists.txt` — сборочная конфигурация бэкенда.

---

## Краткая справка: запуск и smoke-test

```bash
# 1. Собрать сервер
cmake --build build --target cppwiki_server

# 2. Запустить
./build/src/server/cppwiki-server

# 3. Проверить здоровье
curl http://localhost:8080/api/v1/health

# 4. Проверить авторизационный фильтр — без токена должен вернуть 401
curl -X POST http://localhost:8080/api/v1/locks/test-page

# 5. Проверить endpoint с произвольным токеном
curl -H "Authorization: Bearer fake-token" \
     -X POST http://localhost:8080/api/v1/locks/test-page
```
