/* SPDX-License-Identifier: LGPL-2.0-or-later */
#include "table/ApplicationIdentityProvider.h"
#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>
#include <processcore/application_process_model.h>
#include <processcore/process_data_model.h>

class InformationFixture : public QAbstractTableModel {
    Q_OBJECT
public:
    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() || !present ? 0 : 1;
    }
    int columnCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : 9;
    }
    Q_INVOKABLE int rowForApplication(const QString& id) const
    {
        return present && id == QLatin1String("terminal") ? 0 : -1;
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid()) {
            return {};
        }
        using Model = KSysGuard::ApplicationProcessModel;
        if (role == Model::ActionEligible) {
            return true;
        }
        if (role == Model::ActionPIDs) {
            return QVariantList{100, 101};
        }
        if (role == KSysGuard::ProcessDataModel::PIDs) {
            return QVariantList{100, 101, 102, 103};
        }
        if (role == KSysGuard::ProcessDataModel::Value) {
            const QVariantList values{QStringLiteral("Terminal"), QStringLiteral("terminal.desktop"), 25.0, 4096.0,
                80.0, 40.0, 2048.0, 1024.0, QStringLiteral("utilities-terminal")};
            return values.at(index.column());
        }
        return {};
    }
    void setPresent(bool value)
    {
        beginResetModel();
        present = value;
        endResetModel();
        Q_EMIT applicationsChanged();
    }
Q_SIGNALS:
    void applicationsChanged();

private:
    bool present = true;
};

class TestApplicationIdentity : public QObject {
    Q_OBJECT
    QUrl informationUrl() const
    {
        return QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral(APPLICATION_INFORMATION_QML)));
    }
private Q_SLOTS:
    void initTestCase()
    {
        qmlRegisterType<KSysGuard::ApplicationProcessModel>("org.kde.ksysguard.process", 1, 0, "ApplicationProcessModel");
    }

    void localAndRemoteHints()
    {
        const QVariantMap identity{{QStringLiteral("pid"), 100}, {QStringLiteral("startTime"), QStringLiteral("1234")}};
        auto hint = ApplicationIdentityProvider::localHint(identity, "workstation", "workstation", QStringLiteral("editor"), QStringLiteral("Editor"));
        QCOMPARE(hint.value(QStringLiteral("desktopId")).toString(), QStringLiteral("editor"));
        QCOMPARE(hint.value(QStringLiteral("startTime")).toString(), QStringLiteral("1234"));
        QVERIFY(ApplicationIdentityProvider::localHint(identity, "remote", "workstation", {}, {}).isEmpty());
        QVERIFY(ApplicationIdentityProvider::localHint(identity, {}, "workstation", {}, {}).isEmpty());
        QVERIFY(ApplicationIdentityProvider::localHint({}, "workstation", "workstation", {}, {}).isEmpty());
        QVERIFY(!ApplicationIdentityProvider::localHint(identity, "WORKSTATION", "workstation", {}, {}).isEmpty());
    }

    void validatesModelAtQmlBoundary()
    {
        ApplicationIdentityProvider provider;
        QObject unrelated;
        QVERIFY(provider.setProperty("model", QVariant::fromValue<QObject*>(&unrelated)));
        QVERIFY(!provider.model());
        KSysGuard::ApplicationProcessModel model;
        model.setEnabled(false);
        QVERIFY(provider.setProperty("model", QVariant::fromValue<QObject*>(&model)));
        QCOMPARE(provider.model(), &model);
    }

    void stableApplicationInformation()
    {
        // Use a fake identity-keyed model so row movements/removal can be
        // exercised without collecting or signaling live application processes.
        QQmlEngine engine;
        QQmlComponent component(&engine, informationUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> info(component.create());
        QVERIFY(info);
        QCOMPARE(info->property("actionEligible").toBool(), false);
        QCOMPARE(info->property("pids").toList().size(), 0);
        // Missing selections must remain inert, not silently bind another row.
        KSysGuard::ApplicationProcessModel model;
        model.setEnabled(false);
        info->setProperty("model", QVariant::fromValue<QObject*>(&model));
        info->setProperty("applicationId", QStringLiteral("removed.desktop"));
        QCOMPARE(info->property("actionEligible").toBool(), false);
        QCOMPARE(info->property("pids").toList().size(), 0);
    }

    void informationValuesAndAccountingCounts()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, informationUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> info(component.create());
        QVERIFY(info);
        InformationFixture model;
        info->setProperty("model", QVariant::fromValue<QObject*>(&model));
        info->setProperty("role", KSysGuard::ProcessDataModel::Value);
        info->setProperty("pidsRole", KSysGuard::ProcessDataModel::PIDs);
        const QStringList names{QStringLiteral("name"), QStringLiteral("menuId"), QStringLiteral("cpu"), QStringLiteral("memory"),
            QStringLiteral("netInbound"), QStringLiteral("netOutbound"), QStringLiteral("diskRead"),
            QStringLiteral("diskWrite"), QStringLiteral("iconName")};
        QVariantMap columns;
        for (int i = 0; i < names.size(); ++i) {
            columns.insert(names.at(i), i);
        }
        info->setProperty("sensorColumns", columns);
        info->setProperty("applicationId", QStringLiteral("terminal"));
        QCOMPARE(info->property("name").toString(), QStringLiteral("Terminal"));
        QCOMPARE(info->property("menuId").toString(), QStringLiteral("terminal.desktop"));
        QCOMPARE(info->property("iconName").toString(), QStringLiteral("utilities-terminal"));
        QCOMPARE(info->property("cpu").toDouble(), 25.0);
        QCOMPARE(info->property("memory").toDouble(), 4096.0);
        QCOMPARE(info->property("netInbound").toDouble(), 80.0);
        QCOMPARE(info->property("netOutbound").toDouble(), 40.0);
        QCOMPARE(info->property("diskRead").toDouble(), 2048.0);
        QCOMPARE(info->property("diskWrite").toDouble(), 1024.0);
        QVERIFY(info->property("actionEligible").toBool());
        QCOMPARE(info->property("pids").toList().size(), 4);
        QCOMPARE(info->property("actionPids").toList().size(), 2);
        model.setPresent(false);
        QVERIFY(!info->property("actionEligible").toBool());
        QCOMPARE(info->property("pids").toList().size(), 0);
        QCOMPARE(info->property("actionPids").toList().size(), 0);
        model.setPresent(true);
        QCOMPARE(info->property("pids").toList().size(), 4);
        QCOMPARE(info->property("cpu").toDouble(), 25.0);
    }
};

QTEST_MAIN(TestApplicationIdentity)
#include "TestApplicationIdentity.moc"
