// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/web_dialog_view.h"
#include "chrome/browser/ui/webui/test_web_dialog_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using testing::Eq;

namespace {

// Initial size of WebDialog for SizeWindow test case.
const int kInitialWidth = 40;
const int kInitialHeight = 40;

class TestWebDialogView : public WebDialogView {
 public:
  TestWebDialogView(Profile* profile,
                    Browser* browser,
                    WebDialogDelegate* delegate)
      : WebDialogView(profile, browser, delegate),
        painted_(false),
        should_quit_on_size_change_(false) {
    delegate->GetDialogSize(&last_size_);
  }

  bool painted() const {
    return painted_;
  }

  void set_should_quit_on_size_change(bool should_quit) {
    should_quit_on_size_change_ = should_quit;
  }

 private:
  // TODO(xiyuan): Update this when WidgetDelegate has bounds change hook.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE {
    if (should_quit_on_size_change_ && last_size_ != bounds.size()) {
      // Schedule message loop quit because we could be called while
      // the bounds change call is on the stack and not in the nested message
      // loop.
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &MessageLoop::Quit, base::Unretained(MessageLoop::current())));
    }

    last_size_ = bounds.size();
  }

  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    should_quit_on_size_change_ = false;  // No quit when we are closing.
    WebDialogView::OnDialogClosed(json_retval);
  }

  virtual void OnTabMainFrameRender() OVERRIDE {
    WebDialogView::OnTabMainFrameRender();
    painted_ = true;
    MessageLoop::current()->Quit();
  }

  // Whether first rendered notification is received.
  bool painted_;

  // Whether we should quit message loop when size change is detected.
  bool should_quit_on_size_change_;
  gfx::Size last_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWebDialogView);
};

}  // namespace

class WebDialogBrowserTest : public InProcessBrowserTest {
 public:
  WebDialogBrowserTest() {}
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_SizeWindow SizeWindow
#else
// http://code.google.com/p/chromium/issues/detail?id=52602
// Windows has some issues resizing windows- an off by one problem,
// and a minimum size that seems too big.  This file isn't included in
// Mac builds yet. On Chrome OS, this test doesn't apply since ChromeOS
// doesn't allow resizing of windows.
#define MAYBE_SizeWindow DISABLED_SizeWindow
#endif

IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, MAYBE_SizeWindow) {
  test::TestWebDialogDelegate* delegate =
      new test::TestWebDialogDelegate(
          GURL(chrome::kChromeUIChromeURLsURL));
  delegate->set_size(kInitialWidth, kInitialHeight);

  TestWebDialogView* view =
      new TestWebDialogView(browser()->profile(), browser(), delegate);
  WebContents* web_contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(web_contents != NULL);
  views::Widget::CreateWindowWithParent(
      view, web_contents->GetView()->GetTopLevelNativeWindow());
  view->GetWidget()->Show();

  // TestWebDialogView should quit current message loop on size change.
  view->set_should_quit_on_size_change(true);

  gfx::Rect bounds = view->GetWidget()->GetClientAreaScreenBounds();

  gfx::Rect set_bounds = bounds;
  gfx::Rect actual_bounds, rwhv_bounds;

  // Bigger than the default in both dimensions.
  set_bounds.set_width(400);
  set_bounds.set_height(300);

  view->MoveContents(web_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      view->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Larger in one dimension and smaller in the other.
  set_bounds.set_width(550);
  set_bounds.set_height(250);

  view->MoveContents(web_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      view->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Get very small.
  gfx::Size min_size = view->GetWidget()->GetMinimumSize();
  set_bounds.set_size(min_size);

  view->MoveContents(web_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      view->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Check to make sure we can't get to 0x0
  set_bounds.set_width(0);
  set_bounds.set_height(0);

  view->MoveContents(web_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_LT(0, actual_bounds.width());
  EXPECT_LT(0, actual_bounds.height());
}

// This is timing out about 5~10% of runs. See crbug.com/86059.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, DISABLED_WebContentRendered) {
  WebDialogDelegate* delegate = new test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIChromeURLsURL));

  TestWebDialogView* view =
      new TestWebDialogView(browser()->profile(), browser(), delegate);
  WebContents* web_contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(web_contents != NULL);
  views::Widget::CreateWindowWithParent(
      view, web_contents->GetView()->GetTopLevelNativeWindow());
  EXPECT_TRUE(view->initialized_);

  view->InitDialog();
  view->GetWidget()->Show();

  // TestWebDialogView::OnTabMainFrameRender() will Quit().
  MessageLoopForUI::current()->Run();

  EXPECT_TRUE(view->painted());

  view->GetWidget()->Close();
}
