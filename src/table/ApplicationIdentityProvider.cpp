/* SPDX-License-Identifier: LGPL-2.0-or-later */
#include "ApplicationIdentityProvider.h"

#include <KWindowSystem>
#include <QGuiApplication>
#include <QSysInfo>
#include <algorithm>
#if HAVE_APPLICATION_X11
#include <KWindowInfo>
#include <KX11Extras>
#endif

namespace {
bool hasX11Connection()
{
#if HAVE_APPLICATION_X11 && QT_CONFIG(xcb)
    const auto* x11 = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11 && x11->connection() && KWindowSystem::isPlatformX11();
#else
    return false;
#endif
}
}

ApplicationIdentityProvider::ApplicationIdentityProvider(QObject* parent)
    : QObject(parent)
{
    m_changes.setSingleShot(true);
    m_changes.setInterval(50);
    connect(&m_changes, &QTimer::timeout, this, &ApplicationIdentityProvider::refresh);
#if HAVE_APPLICATION_X11
    if (hasX11Connection()) {
        auto* windows = KX11Extras::self();
        connect(windows, &KX11Extras::windowAdded, this, [this](WId) { schedule(); });
        connect(windows, &KX11Extras::windowRemoved, this, [this](WId id) { m_windows.remove(id); schedule(); });
        connect(windows, &KX11Extras::windowChanged, this, [this](WId, NET::Properties properties, NET::Properties2 properties2) {
            if (properties.testFlag(NET::WMPid) || properties.testFlag(NET::WMWindowType)
                || properties2.testFlag(NET::WM2DesktopFileName) || properties2.testFlag(NET::WM2GTKApplicationId)
                || properties2.testFlag(NET::WM2WindowClass) || properties2.testFlag(NET::WM2ClientMachine)) {
                schedule();
            }
        });
    }
#endif
}

KSysGuard::ApplicationProcessModel* ApplicationIdentityProvider::model() const
{
    return m_model;
}
QObject* ApplicationIdentityProvider::modelObject() const
{
    return m_model;
}
void ApplicationIdentityProvider::setModelObject(QObject* model)
{
    setModel(qobject_cast<KSysGuard::ApplicationProcessModel*>(model));
}
void ApplicationIdentityProvider::setModel(KSysGuard::ApplicationProcessModel* model)
{
    if (model == m_model) {
        return;
    }
    if (m_model) {
        m_model->setIdentityHints({});
    }
    m_windows.clear();
    m_model = model;
    schedule();
    Q_EMIT modelChanged();
}
bool ApplicationIdentityProvider::enabled() const
{
    return m_enabled;
}
void ApplicationIdentityProvider::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (enabled) {
        schedule();
    } else {
        m_changes.stop();
    }
    Q_EMIT enabledChanged();
}
void ApplicationIdentityProvider::schedule()
{
    if (m_enabled && m_model && !m_changes.isActive()) {
        m_changes.start();
    }
}

QVariantMap ApplicationIdentityProvider::localHint(const QVariantMap& identity, const QByteArray& clientMachine, const QByteArray& localMachine,
    const QString& desktopId, const QString& windowClass)
{
    const auto host = clientMachine.toLower();
    const auto local = localMachine.toLower();
    if (host.isEmpty() || local.isEmpty() || (host != local && host != "localhost" && host != "localhost.localdomain")
        || identity.value(QStringLiteral("pid")).toLongLong() <= 1 || identity.value(QStringLiteral("startTime")).toLongLong() <= 0) {
        return {};
    }
    auto hint = identity;
    hint.insert(QStringLiteral("desktopId"), desktopId);
    hint.insert(QStringLiteral("windowClass"), windowClass);
    return hint;
}

void ApplicationIdentityProvider::refresh()
{
    if (!m_enabled || !m_model) {
        return;
    }
    QVariantList hints;
#if HAVE_APPLICATION_X11
    if (hasX11Connection()) {
        QHash<quintptr, QVariantMap> windows;
        auto ids = KX11Extras::windows();
        std::sort(ids.begin(), ids.end());
        for (const auto id : ids) {
            KWindowInfo info(id, NET::WMPid | NET::WMWindowType,
                NET::WM2ClientMachine | NET::WM2DesktopFileName | NET::WM2GTKApplicationId | NET::WM2WindowClass);
            if (!info.valid() || info.windowType(NET::AllTypesMask) == NET::Desktop || info.windowType(NET::AllTypesMask) == NET::Dock) {
                continue;
            }
            const auto pid = info.pid();
            // Bind an existing window to its original incarnation. A stale X11
            // PID must not become a hint for an unrelated recycled local PID.
            auto identity = m_windows.value(id);
            if (identity.value(QStringLiteral("pid")).toLongLong() != pid) {
                identity = m_model->processIdentity(pid);
            }
            const auto desktopId = info.desktopFileName().isEmpty() ? info.gtkApplicationId() : info.desktopFileName();
            const auto hint = localHint(identity, info.clientMachine(), QSysInfo::machineHostName().toUtf8(),
                QString::fromUtf8(desktopId), QString::fromUtf8(info.windowClassClass()));
            if (!hint.isEmpty()) {
                windows.insert(id, hint);
                hints.append(hint);
            }
        }
        m_windows = windows;
    }
#endif
    m_model->setIdentityHints(hints);
}
