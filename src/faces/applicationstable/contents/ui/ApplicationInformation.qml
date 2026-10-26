/*
 * SPDX-FileCopyrightText: 2020 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQml
import org.kde.ksysguard.process as Process

QtObject {
    id: info

    property var model
    property string applicationId
    property bool actionEligible: false
    onApplicationIdChanged: info.update()

    property var sensorColumns
    property int role
    property int pidsRole

    property string name
    property string menuId
    property real cpu
    property real memory
    property real netInbound
    property real netOutbound
    property real diskRead
    property real diskWrite

    property string cpuMode

    property string iconName

    property var pids: []
    property var actionPids: []

    property var connection: Connections {
        target: model
        function onDataChanged() { if (info) { info.update() } }
        function onApplicationsChanged() { if (info) { info.update() } }
        function onEnabledAttributesChanged() { if (info) { info.update() } }
    }

    function update() {
        if (model == null || applicationId.length === 0) {
            return
        }

        const row = model.rowForApplication(applicationId)
        if (row < 0) {
            actionEligible = false
            pids = []
            actionPids = []
            return
        }
        const index = model.index(row, 0)
        actionEligible = model.data(index, Process.ApplicationProcessModel.ActionEligible) === true

        const readValue = sensor => {
            const column = sensorColumns[sensor]
            return column >= 0 ? model.data(model.index(row, column), role) : undefined
        }
        name = "" + (readValue("name") ?? "")
        menuId = "" + (readValue("menuId") ?? "")
        cpu = parseFloat(readValue("cpu"))
        memory = parseFloat(readValue("memory"))
        netInbound = parseFloat(readValue("netInbound"))
        netOutbound = parseFloat(readValue("netOutbound"))
        diskRead = parseFloat(readValue("diskRead"))
        diskWrite = parseFloat(readValue("diskWrite"))
        iconName = "" + (readValue("iconName") ?? "")

        pids = model.data(model.index(index.row, 0), pidsRole)
        actionPids = model.data(index, Process.ApplicationProcessModel.ActionPIDs) ?? []
        if (!pids) {
            pids = []
        }
    }
}
