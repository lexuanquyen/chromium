// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/home_page_overlay_handler2.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options2 {

HomePageOverlayHandler::HomePageOverlayHandler()
    : autocomplete_controller_(NULL) {
}

HomePageOverlayHandler::~HomePageOverlayHandler() {
}

void HomePageOverlayHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestAutocompleteSuggestionsForHomePage",
      base::Bind(&HomePageOverlayHandler::RequestAutocompleteSuggestions,
                 base::Unretained(this)));
}

void HomePageOverlayHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  autocomplete_controller_.reset(new AutocompleteController(profile, this));
}

void HomePageOverlayHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  RegisterTitle(localized_strings, "homePageOverlay",
                IDS_OPTIONS2_HOMEPAGE_TITLE);
}

void HomePageOverlayHandler::RequestAutocompleteSuggestions(
    const ListValue* args) {
  string16 input;
  CHECK_EQ(args->GetSize(), 1U);
  CHECK(args->GetString(0, &input));

  autocomplete_controller_->Start(input, string16(), true, false, false,
                                  AutocompleteInput::ALL_MATCHES);
}

void HomePageOverlayHandler::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  base::ListValue suggestions;
  OptionsUI::ProcessAutocompleteSuggestions(result, &suggestions);
  web_ui()->CallJavascriptFunction(
      "HomePageOverlay.updateAutocompleteSuggestions", suggestions);
}

}  // namespace options2
