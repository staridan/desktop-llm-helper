#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QObject>
#include <QString>

#include <QAbstractNativeEventFilter>
#include <windows.h>

/**
 * @brief Управляет глобальным хоткеем: регистрирует его и перехватывает
 *        низко-уровневым хуком WH_KEYBOARD_LL, окончательно блокируя
 *        дальнейшую обработку системой.
 *
 *  ❕ Работает только на Windows.
 */
class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit HotkeyManager(QObject *parent = nullptr);

    ~HotkeyManager() override;

    /**
     * @brief Регистрирует сочетание клавиш (пример: "Ctrl+Alt+H").
     * @return true, если удалось запустить хук.
     *
     *  Используется только low-level-hook: хоткей не «просачивается» дальше.
     */
    bool registerHotkey(const QString &sequence);

    /**
     * @note Реализовано для совместимости, но не используется, так как
     *       WM_HOTKEY здесь не генерируется. Всегда возвращает false.
     */
    bool nativeEventFilter(const QByteArray &eventType,
                           void *message,
                           qintptr *result) override;

signals:
    /// Сигнал испускается при нажатии зарегистрированного хоткея
    void hotkeyPressed();

private:
    int id; ///< (не используется) id для RegisterHotKey
    UINT currentModifiers; ///< модификаторы (MOD_…)
    UINT currentVk; ///< виртуальный код основной клавиши

    /// Статический low-level-hook и указатель на активный менеджер
    static HHOOK s_hook;
    static HotkeyManager *s_instance;

    static LRESULT CALLBACK LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam);

    bool parseSequence(const QString &sequence, UINT &modifiers, UINT &vk) const;
};

#endif // HOTKEYMANAGER_H
