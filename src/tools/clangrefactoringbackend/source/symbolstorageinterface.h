/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "sourcelocationentry.h"
#include "symbolentry.h"

#include <filestatus.h>
#include <sourceentry.h>

#include <compilermacro.h>
#include <sqlitetransaction.h>

#include <utils/cpplanguage_details.h>

namespace ClangBackEnd {

class SymbolStorageInterface
{
public:
    SymbolStorageInterface() = default;
    SymbolStorageInterface(const SymbolStorageInterface &) = delete;
    SymbolStorageInterface &operator=(const SymbolStorageInterface &) = delete;

    virtual void addSymbolsAndSourceLocations(const SymbolEntries &symbolEntries,
                                              const SourceLocationEntries &sourceLocations)
        = 0;
    virtual void insertOrUpdateIndexingTimeStamps(const FilePathIds &filePathIds, TimeStamp indexingTimeStamp) = 0;
    virtual void insertOrUpdateIndexingTimeStamps(const FileStatuses &fileStatuses) = 0;
    virtual SourceTimeStamps fetchIndexingTimeStamps() const = 0;
    virtual SourceTimeStamps fetchIncludedIndexingTimeStamps(FilePathId sourcePathId) const = 0;
    virtual FilePathIds fetchDependentSourceIds(const FilePathIds &sourcePathIds) const = 0;

protected:
    ~SymbolStorageInterface() = default;
};

} // namespace ClangBackEnd
