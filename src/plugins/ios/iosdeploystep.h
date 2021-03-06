/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "iosconfigurations.h"
#include "iosdevice.h"
#include "iossimulator.h"

#include <projectexplorer/buildstep.h>
#include <projectexplorer/devicesupport/idevice.h>

#include <QProcess>

namespace Ios {
class IosToolHandler;
namespace Internal {

class IosDeployStep : public ProjectExplorer::BuildStep
{
    Q_OBJECT
public:
    enum TransferStatus {
        NoTransfer,
        TransferInProgress,
        TransferOk,
        TransferFailed
    };

    friend class IosDeployStepFactory;
    explicit IosDeployStep(ProjectExplorer::BuildStepList *bc);
    static Core::Id stepId();

    void cleanup();
private:
    void doRun() override;
    void doCancel() override;
    bool fromMap(const QVariantMap &map) override;
    QVariantMap toMap() const override;

    void handleIsTransferringApp(Ios::IosToolHandler *handler, const QString &bundlePath,
                           const QString &deviceId, int progress, int maxProgress,
                           const QString &info);
    void handleDidTransferApp(Ios::IosToolHandler *handler, const QString &bundlePath, const QString &deviceId,
                        Ios::IosToolHandler::OpStatus status);
    void handleFinished(Ios::IosToolHandler *handler);
    void handleErrorMsg(Ios::IosToolHandler *handler, const QString &msg);
    void updateDisplayNames();

    bool init() override;
    ProjectExplorer::BuildStepConfigWidget *createConfigWidget() override;
    ProjectExplorer::IDevice::ConstPtr device() const;
    IosDevice::ConstPtr iosdevice() const;
    IosSimulator::ConstPtr iossimulator() const;

    QString deviceId() const;
    void raiseError(const QString &error);
    void writeOutput(const QString &text, OutputFormat = OutputFormat::NormalMessage);
    void checkProvisioningProfile();

    TransferStatus m_transferStatus = NoTransfer;
    IosToolHandler *m_toolHandler = nullptr;
    ProjectExplorer::IDevice::ConstPtr m_device;
    Utils::FileName m_bundlePath;
    IosDeviceType m_deviceType;
    static const Core::Id Id;
    bool m_expectFail = false;
};

} // namespace Internal
} // namespace Ios
