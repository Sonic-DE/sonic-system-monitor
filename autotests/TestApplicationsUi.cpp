/* SPDX-License-Identifier: LGPL-2.0-or-later */
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedQmlContext>
#include <KUser>
#include <QDir>
#include <QFile>
#include <QItemSelectionModel>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QTest>
#include <faces/SensorFaceController.h>
#include <processcore/application_controller.h>
#include <processcore/process_data_model.h>

// Exercise the real QML proxy/filter chain through the existing private source
// seam. Production QML has no writable filesystem-root property.
class ApplicationDiscoveryTest {
public:
    static void useRoots(KSysGuard::ApplicationProcessModel* model, const QString& proc, const QString& groups)
    {
        model->setDiscoveryRoots(proc, groups);
    }
};

class TestApplicationsUi : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void loadsIndependentApplicationsFace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedQmlContext(&engine));
        KConfig config(directory.filePath(QStringLiteral("test.conf")), KConfig::SimpleConfig);
        KConfigGroup group(&config, QStringLiteral("Applications"));
        KSysGuard::SensorFaceController face(group, &engine, &engine);
        face.setFaceId(QStringLiteral("org.kde.ksysguard.applicationstable"));
        auto* item = face.fullRepresentation();
        QVERIFY(item);
        auto* table = item->findChild<QQuickItem*>(QStringLiteral("applicationsTable"));
        QVERIFY(table);
        auto* controller = item->findChild<KSysGuard::ApplicationController*>();
        QVERIFY(controller);
        QVERIFY(controller->model());
        auto* model = controller->model();
        item->setVisible(true);
        table->setVisible(true);
        model->setEnabled(true);
        model->update();
        QVERIFY(model->available());
        QVERIFY(model->availableAttributes().contains(QStringLiteral("appName")));
        for (const auto& attribute : {QStringLiteral("totalUserUsage"), QStringLiteral("totalSysUsage"), QStringLiteral("totalUsage")}) {
            QVERIFY(model->availableAttributes().contains(attribute));
        }

        // Selecting the live background bucket is safe: no application control
        // test ever dispatches to an actual user's application processes.
        const auto background = model->rowForApplication(QStringLiteral("services"));
        QVERIFY(controller->capture({QStringLiteral("services")}).isEmpty());
        auto* selection = qvariant_cast<QItemSelectionModel*>(table->property("selection"));
        QVERIFY(selection);
        auto* proxy = selection->model();
        bool selected = false;
        for (int row = 0; row < proxy->rowCount(); ++row) {
            const auto index = proxy->index(row, 0);
            if (index.data(KSysGuard::ApplicationProcessModel::ApplicationId).toString() == QLatin1String("services")) {
                selection->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                selected = true;
                break;
            }
        }
        if (background >= 0 && !model->index(background, 0).data(KSysGuard::ProcessDataModel::PIDs).toList().isEmpty()) {
            QVERIFY(selected);
            QTRY_VERIFY(!item->property("quitEnabled").toBool());
        }
        delete item; // Tear down QML bindings before their controller/config.
    }

    void visibleCountsTrackDiscoveryAndHideEmptyUnits()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto uid = KUserId::currentEffectiveUserId().nativeId();
        const auto proc = directory.filePath(QStringLiteral("proc"));
        const auto groups = directory.filePath(QStringLiteral("groups"));
        const auto desktop = QStringLiteral("/user.slice/user-%1.slice/user@%1.service").arg(uid);
        const auto app = desktop + QStringLiteral("/app.slice/app-org.test.ViewA-one.scope");
        const auto background = desktop + QStringLiteral("/background.slice/fixture.service");
        const auto empty = desktop + QStringLiteral("/app.slice/app-org.test.ViewEmpty-one.scope");
        const auto write = [](const QString& path, const QByteArray& bytes) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
        };
        const auto process = [&](qint64 pid, const QString& name, const QString& group) {
            const auto path = proc + QLatin1Char('/') + QString::number(pid);
            if (!QDir().mkpath(path)) {
                return false;
            }
            QByteArrayList fields;
            for (int i = 0; i < 20; ++i) {
                fields.append("0");
            }
            fields[0] = "S";
            fields[1] = "1";
            fields[2] = fields[3] = QByteArray::number(pid);
            fields[19] = QByteArray::number(pid * 10);
            return write(path + QStringLiteral("/stat"), QByteArray::number(pid) + " (" + name.toUtf8() + ") " + fields.join(' ') + '\n')
                && write(path + QStringLiteral("/status"), "Uid:\t" + QByteArray::number(uid) + '\t' + QByteArray::number(uid) + "\t0\t0\n")
                && write(path + QStringLiteral("/cgroup"), "0::" + group.toUtf8() + '\n');
        };
        for (const auto& group : {app, background, empty}) {
            QVERIFY(QDir().mkpath(groups + group));
        }
        QVERIFY(process(9000010, QStringLiteral("ViewA"), app));
        QVERIFY(process(9000020, QStringLiteral("Background"), background));
        QVERIFY(write(groups + app + QStringLiteral("/cgroup.procs"), "9000010\n"));
        QVERIFY(write(groups + background + QStringLiteral("/cgroup.procs"), "9000020\n"));
        QVERIFY(write(groups + empty + QStringLiteral("/cgroup.procs"), {}));

        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedQmlContext(&engine));
        KConfig config(directory.filePath(QStringLiteral("fixture.conf")), KConfig::SimpleConfig);
        KConfigGroup configuration(&config, QStringLiteral("Applications"));
        KSysGuard::SensorFaceController face(configuration, &engine, &engine);
        face.setFaceId(QStringLiteral("org.kde.ksysguard.applicationstable"));
        auto* item = face.fullRepresentation();
        QVERIFY(item);
        auto* table = item->findChild<QQuickItem*>(QStringLiteral("applicationsTable"));
        auto* controller = item->findChild<KSysGuard::ApplicationController*>();
        QVERIFY(table);
        QVERIFY(controller);
        auto* model = controller->model();
        QVERIFY(model);
        model->setEnabled(false);
        ApplicationDiscoveryTest::useRoots(model, proc, groups);
        item->setVisible(true);
        table->setVisible(true);
        model->setEnabled(true);
        model->update();
        auto* selection = qvariant_cast<QItemSelectionModel*>(table->property("selection"));
        QVERIFY(selection);
        auto* proxy = selection->model();
        QTRY_COMPARE(model->rowCount(), 3);
        QTRY_COMPARE(proxy->rowCount(), 2);
        QVERIFY(model->rowForApplication(QStringLiteral("org.test.ViewEmpty")) >= 0);
        QSet<QString> visible;
        for (int row = 0; row < proxy->rowCount(); ++row) {
            visible.insert(proxy->index(row, 0).data(KSysGuard::ApplicationProcessModel::ApplicationId).toString());
        }
        QCOMPARE(visible, QSet<QString>({QStringLiteral("services"), QStringLiteral("org.test.ViewA")}));
        QVERIFY(controller->capture({QStringLiteral("services")}).isEmpty());

        QVERIFY(process(9000030, QStringLiteral("ViewEmpty"), empty));
        QVERIFY(write(groups + empty + QStringLiteral("/cgroup.procs"), "9000030\n"));
        model->update();
        QTRY_COMPARE(model->rowCount(), 3);
        QTRY_COMPARE(proxy->rowCount(), 3);

        QVERIFY(QDir(proc + QStringLiteral("/9000010")).removeRecursively());
        QVERIFY(write(groups + app + QStringLiteral("/cgroup.procs"), {}));
        model->update();
        QTRY_COMPARE(model->rowCount(), 3);
        QTRY_COMPARE(proxy->rowCount(), 2);
        delete item;
    }
};

QTEST_MAIN(TestApplicationsUi)
#include "TestApplicationsUi.moc"
