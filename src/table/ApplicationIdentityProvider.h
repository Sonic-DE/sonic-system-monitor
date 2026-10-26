/* SPDX-License-Identifier: LGPL-2.0-or-later */
#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariant>
#include <processcore/application_process_model.h>
#include <qqmlintegration.h>

/** Optional X11 identity input. Process discovery remains in ProcessCore. */
class ApplicationIdentityProvider : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* model READ modelObject WRITE setModelObject NOTIFY modelChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit ApplicationIdentityProvider(QObject* parent = nullptr);
    KSysGuard::ApplicationProcessModel* model() const;
    void setModel(KSysGuard::ApplicationProcessModel* model);
    bool enabled() const;
    void setEnabled(bool enabled);
    // Pure sanitization seam used by tests. A remote or absent client-machine
    // identity must never associate an arbitrary host PID with a local process.
    static QVariantMap localHint(const QVariantMap& identity, const QByteArray& clientMachine, const QByteArray& localMachine,
        const QString& desktopId, const QString& windowClass);
Q_SIGNALS:
    void modelChanged();
    void enabledChanged();

private:
    // Keep the QML boundary independent of another module's C++ type
    // metadata, while validating and retaining a strongly typed model inside.
    QObject* modelObject() const;
    void setModelObject(QObject* model);
    void refresh();
    void schedule();
    QPointer<KSysGuard::ApplicationProcessModel> m_model;
    QTimer m_changes;
    bool m_enabled = true;
    QHash<quintptr, QVariantMap> m_windows;
};
