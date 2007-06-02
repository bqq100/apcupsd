// This file has been adapted to the Win32 version of Apcupsd
// by Kern E. Sibbald.  Many thanks to ATT and James Weatherall,
// the original author, for providing an excellent template.
//
// Rewrite/Refactoring by Adam Kropelin
//
// Copyright (2007) Adam D. Kropelin
// Copyright (2000-2005) Kern E. Sibbald

// Implementation of a system tray icon & menu for Apcupsd

#include "apc.h"
#include <windows.h>
#include "winups.h"
#include "winres.h"
#include "wintray.h"

extern char *ups_status(int stat);
extern int battstat;

// Implementation


// Implementation
upsMenu::upsMenu(HINSTANCE appinst)
   : m_about(appinst),
     m_status(appinst),
     m_events(appinst)
{
   // Create a dummy window to handle tray icon messages
   WNDCLASSEX wndclass;
   wndclass.cbSize = sizeof(wndclass);
   wndclass.style = 0;
   wndclass.lpfnWndProc = upsMenu::WndProc;
   wndclass.cbClsExtra = 0;
   wndclass.cbWndExtra = 0;
   wndclass.hInstance = appinst;
   wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
   wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
   wndclass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
   wndclass.lpszMenuName = (const char *)NULL;
   wndclass.lpszClassName = APCTRAY_WINDOW_CLASS;
   wndclass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
   RegisterClassEx(&wndclass);

   // Create System Tray menu Window
   m_hwnd = CreateWindow(APCTRAY_WINDOW_CLASS, APCTRAY_WINDOW_NAME,
                         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                         CW_USEDEFAULT, 200, 200, NULL, NULL, appinst, NULL);
   if (m_hwnd == NULL) {
      PostQuitMessage(0);
      return;
   }

   // record which client created this window
   SetWindowLong(m_hwnd, GWL_USERDATA, (LONG)this);

   // Timer to trigger icon updating
   SetTimer(m_hwnd, 1, 1000, NULL);

   // No balloon timer yet
   m_balloon_timer = 0;

   // Load the icons for the tray
   m_online_icon = LoadIcon(appinst, MAKEINTRESOURCE(IDI_ONLINE));
   m_onbatt_icon = LoadIcon(appinst, MAKEINTRESOURCE(IDI_ONBATT));
   m_charging_icon = LoadIcon(appinst, MAKEINTRESOURCE(IDI_CHARGING));
   m_commlost_icon = LoadIcon(appinst, MAKEINTRESOURCE(IDI_COMMLOST));

   // Load the popup menu
   m_hmenu = LoadMenu(appinst, MAKEINTRESOURCE(IDR_TRAYMENU));

   // Install the tray icon!
   AddTrayIcon();
}

upsMenu::~upsMenu()
{
   // Remove the tray icon
   SendTrayMsg(NIM_DELETE);

   // Destroy the loaded menu
   if (m_hmenu != NULL)
      DestroyMenu(m_hmenu);
}

void upsMenu::AddTrayIcon()
{
   SendTrayMsg(NIM_ADD);
}

void upsMenu::DelTrayIcon()
{
   SendTrayMsg(NIM_DELETE);
}

void upsMenu::UpdateTrayIcon()
{
   (void *)ups_status(0);
   SendTrayMsg(NIM_MODIFY);
}

void upsMenu::SendTrayMsg(DWORD msg)
{
   char *stat;

   // Create the tray icon message
   m_nid.hWnd = m_hwnd;
   m_nid.cbSize = sizeof(m_nid);
   m_nid.uID = IDI_APCUPSD;        // never changes after construction

   /* If battstat == 0 we are on batteries, otherwise we are online
    * and the value of battstat is the percent charge.
    */
   if (battstat == -1)
      m_nid.hIcon = m_commlost_icon;
   else if (battstat == 0)
      m_nid.hIcon = m_onbatt_icon;
   else if (battstat >= 100)
      m_nid.hIcon = m_online_icon;
   else
      m_nid.hIcon = m_charging_icon;

   m_nid.uFlags = NIF_ICON | NIF_MESSAGE;
   m_nid.uCallbackMessage = WM_TRAYNOTIFY;

   // Get current status
   stat = ups_status(0);

   // Use status as normal tooltip
   asnprintf(m_nid.szTip, sizeof(m_nid.szTip), "Apcupsd - %s", stat);
   m_nid.uFlags |= NIF_TIP;

   // Send the message
   if (Shell_NotifyIcon(msg, &m_nid)) {
      EnableMenuItem(m_hmenu, ID_CLOSE, MF_ENABLED);
   } else if (msg == NIM_ADD) {
      // The tray icon couldn't be created, so use the Properties dialog
      // as the main program window
      PostQuitMessage(0);
   }
}

// Process window messages
LRESULT CALLBACK upsMenu::WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   // This is a static method, so we don't know which instantiation we're 
   // dealing with. We use Allen Hadden's (ahadden@taratec.com) suggestion 
   // from a newsgroup to get the pseudo-this.
   upsMenu *_this = (upsMenu *) GetWindowLong(hwnd, GWL_USERDATA);

   switch (iMsg) {

   // Timer expired
   case WM_TIMER:
      if (wParam == 1) {
         // Update the icon
         _this->UpdateTrayIcon();
      } else if (wParam == 2) {
         // Balloon timer expired; clear the balloon
         KillTimer(_this->m_hwnd, _this->m_balloon_timer);

         NOTIFYICONDATA nid;
         nid.hWnd = _this->m_hwnd;
         nid.cbSize = sizeof(nid);
         nid.uID = IDI_APCUPSD;
         nid.uFlags = NIF_INFO;
         nid.uTimeout = 0;
         nid.szInfoTitle[0] = '\0';
         nid.szInfo[0] = '\0';
         nid.dwInfoFlags = 0;

         Shell_NotifyIcon(NIM_MODIFY, &nid);
      }
      break;

      // STANDARD MESSAGE HANDLING
   case WM_CREATE:
      return 0;

   case WM_COMMAND:
      // User has clicked an item on the tray menu
      switch (LOWORD(wParam)) {

      case ID_STATUS:
         // Show the status dialog
         _this->m_status.Show(TRUE);
         _this->UpdateTrayIcon();
         break;

      case ID_EVENTS:
         // Show the Events dialog
         _this->m_events.Show(TRUE);
         _this->UpdateTrayIcon();
         break;

      case ID_ABOUT:
         // Show the About box
         _this->m_about.Show(TRUE);
         break;

      case ID_CLOSE:
         // User selected Close from the tray menu
         PostMessage(hwnd, WM_CLOSE, 0, 0);
         break;

      }
      return 0;

   case WM_TRAYNOTIFY:
      // User has clicked on the tray icon or the menu
      {
         // Get the submenu to use as a pop-up menu
         HMENU submenu = GetSubMenu(_this->m_hmenu, 0);

         // What event are we responding to, RMB click?
         if (lParam == WM_RBUTTONUP) {
            if (submenu == NULL) {
               return 0;
            }
            // Make the first menu item the default (bold font)
            SetMenuDefaultItem(submenu, 0, TRUE);

            // Get the current cursor position, to display the menu at
            POINT mouse;
            GetCursorPos(&mouse);

            // There's a "bug"
            // (Microsoft calls it a feature) in Windows 95 that requires calling
            // SetForegroundWindow. To find out more, search for Q135788 in MSDN.
            //
            SetForegroundWindow(_this->m_nid.hWnd);

            // Display the menu at the desired position
            TrackPopupMenu(submenu, 0, mouse.x, mouse.y, 0, _this->m_nid.hWnd, NULL);

            return 0;
         }

         // Or was there a LMB double click?
         if (lParam == WM_LBUTTONDBLCLK) {
            // double click: execute first menu item
            SendMessage(_this->m_nid.hWnd, WM_COMMAND, GetMenuItemID(submenu, 0), 0);
         }

         return 0;
      }

   case WM_BALLOONSHOW:
      // A balloon notice was shown, so set a timer to clear it
      if (_this->m_balloon_timer != 0)
         KillTimer(_this->m_hwnd, _this->m_balloon_timer);
      _this->m_balloon_timer = SetTimer(_this->m_hwnd, 2, wParam, NULL);
      return 0;

   case WM_CLOSE:
      break;

   case WM_DESTROY:
      // The user wants Apcupsd to quit cleanly...
      PostQuitMessage(0);
      return 0;

   case WM_QUERYENDSESSION:
      // Is the system shutting down
      if (lParam == 0) {
         // No, so we are about to be killed

         // If there are remote connections then we should verify
         // that the user is happy about killing them.

         // Finally, post a quit message, just in case
         PostQuitMessage(0);
      }
      // Tell the OS that we've handled it anyway
      return TRUE;
   }

   // Unknown message type
   return DefWindowProc(hwnd, iMsg, wParam, lParam);
}
