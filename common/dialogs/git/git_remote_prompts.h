/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GIT_REMOTE_PROMPTS_H
#define GIT_REMOTE_PROMPTS_H

#include <functional>
#include <memory>

#include <wx/string.h>

class KIGIT_COMMON;
class WX_PROGRESS_REPORTER;
class wxWindow;

/**
 * Ask the user for the remote's credentials after it rejected the ones we had.
 *
 * On success the answers are stored on the common, the failure flag and the exhausted
 * authentication methods are reset, and the caller may retry the operation that failed.
 * Every remote operation (fetch, pull, push, sync) hits the same wall, so they all ask the
 * same question here.
 *
 * @param aParent Parent window for the dialog.
 * @param aCommon Common carrying the remote and the credentials to update.
 * @return True if the user supplied credentials and the operation may be retried.
 */
bool PromptForGitCredentials( wxWindow* aParent, KIGIT_COMMON* aCommon );

/**
 * Push to the remote, putting to the user the two questions a rejected push raises.
 *
 * A push is rejected either because the remote did not accept our credentials, or because
 * the branch has diverged from its upstream - typically after amending a commit that was
 * already pushed.  Neither is an error the caller can resolve on its own, so both are put
 * to the user and the push retried, and every entry point that pushes (the project tree and
 * the Git check job) goes through here so that the experience is the same from both.
 *
 * @param aParent Parent window for the prompts.
 * @param aCommon Common for the repository to push.
 * @param aProgressFactory Called to build the progress reporter for each attempt.  May be
 *                         empty, and is by callers that already hold a progress dialog of
 *                         their own.
 * @param aError Filled with every reason the push failed, joined; libgit2 usually stacks a
 *               transport error under the message that names the branch, so reporting only
 *               the last one drops the half that says what went wrong.  Left empty when the
 *               push failed without recording a reason.
 * @return True when the push succeeded, false when it failed or the user cancelled.
 */
bool PushWithPrompts( wxWindow* aParent, KIGIT_COMMON* aCommon,
                      const std::function<std::unique_ptr<WX_PROGRESS_REPORTER>()>& aProgressFactory,
                      wxString* aError );

#endif // GIT_REMOTE_PROMPTS_H
