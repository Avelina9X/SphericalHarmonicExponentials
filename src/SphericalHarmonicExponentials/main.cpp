#include "pch.hpp"

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#include "Application.hpp"

// Enable the Agility SDK components
extern "C"
{
	__declspec( dllexport ) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
	__declspec( dllexport ) extern const char* D3D12SDKPath = reinterpret_cast<const char*>( u8".\\D3D12\\" );
}

// Globals
namespace
{
	RECT gWindowRect;
}

// Forward declerations
static int sRun( Application *pApplication, HINSTANCE hInstance, int nCmdShow );
static LRESULT CALLBACK sWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );


// Entrypoint
int APIENTRY wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );

	std::unique_ptr<Application> application = std::make_unique<Application>();

	return sRun( application.get(), hInstance, nCmdShow);
}

int sRun( Application *pApplication, HINSTANCE hInstance, int nCmdShow )
{
	// Verify CPU support
	if ( !DirectX::XMVerifyCPUSupport() ) return 1;

	// Intialise multithreaded support
	if ( FAILED( RoInitialize( RO_INIT_MULTITHREADED ) ) ) return 1;

	// Initialize the window class
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof( WNDCLASSEX );
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = sWindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	windowClass.lpszClassName = L"Hello12Class";
	windowClass.hbrBackground = (HBRUSH) ( COLOR_WINDOW + 1 );
	RegisterClassEx( &windowClass );

	// Adjust window size
	RECT windowRect = { 0, 0, 0, 0 };
	pApplication->GetDefaultSize( windowRect.right, windowRect.bottom );
	AdjustWindowRect( &windowRect, WS_OVERLAPPEDWINDOW, FALSE );

	// Create the window and store a handle to it
	HWND gHwnd = CreateWindow(
		windowClass.lpszClassName,
		L"Paceholder",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		hInstance,
		pApplication
	);

	ShowWindow( gHwnd, nCmdShow );
	GetClientRect( gHwnd, &windowRect );

	// Initialize application
	pApplication->Initialize( gHwnd, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top );

	// Main loop
	MSG msg = {};
	while ( msg.message != WM_QUIT )
	{
		// Process messages in the queue
		if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else {
			pApplication->Tick();
		}
	}

	// Shutdown
	pApplication->OnDestroy();

	return static_cast<char>( msg.wParam );
}

LRESULT sWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	static bool s_in_sizemove = false;
	static bool s_in_suspend = false;
	static bool s_minimized = false;
	static bool s_fullscreen = false;
	// TODO: Set s_fullscreen to true if defaulting to fullscreen.

	auto app = reinterpret_cast<Application *>( GetWindowLongPtr( hWnd, GWLP_USERDATA ) );

	extern LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
	if ( ImGui_ImplWin32_WndProcHandler( hWnd, message, wParam, lParam ) )
		return true;

	switch ( message )
	{
		case WM_CREATE:
			if ( lParam )
			{
				auto params = reinterpret_cast<LPCREATESTRUCTW>( lParam );
				SetWindowLongPtr( hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( params->lpCreateParams ) );
			}
			break;

		case WM_PAINT:
			if ( s_in_sizemove && app )
			{
				app->Tick();
			}
			else
			{
				PAINTSTRUCT ps;
				std::ignore = BeginPaint( hWnd, &ps );
				EndPaint( hWnd, &ps );
			}
			break;

		case WM_SIZE:
			if ( wParam == SIZE_MINIMIZED )
			{
				if ( !s_minimized )
				{
					s_minimized = true;
					if ( !s_in_suspend && app )
						app->OnSuspending();
					s_in_suspend = true;
				}
			}
			else if ( s_minimized )
			{
				s_minimized = false;
				if ( s_in_suspend && app )
					app->OnResuming();
				s_in_suspend = false;
			}
			else if ( !s_in_sizemove && app )
			{
				app->OnWindowSizeChanged( LOWORD( lParam ), HIWORD( lParam ) );
			}
			break;

		case WM_ENTERSIZEMOVE:
			s_in_sizemove = true;
			break;

		case WM_EXITSIZEMOVE:
			s_in_sizemove = false;
			if ( app )
			{
				RECT rc;
				GetClientRect( hWnd, &rc );

				app->OnWindowSizeChanged( rc.right - rc.left, rc.bottom - rc.top );

				// Save rect for restore
				GetWindowRect( hWnd, &gWindowRect );
				SetWindowPos(
					hWnd,
					HWND_TOP,
					gWindowRect.left + 1,
					gWindowRect.top + 1,
					gWindowRect.right - gWindowRect.left,
					gWindowRect.bottom - gWindowRect.top,
					SWP_NOZORDER | SWP_FRAMECHANGED
				);
				SetWindowPos(
					hWnd,
					HWND_TOP,
					gWindowRect.left,
					gWindowRect.top,
					gWindowRect.right - gWindowRect.left,
					gWindowRect.bottom - gWindowRect.top,
					SWP_NOZORDER | SWP_FRAMECHANGED
				);
			}
			break;

		case WM_GETMINMAXINFO:
			if ( lParam )
			{
				auto info = reinterpret_cast<MINMAXINFO*>( lParam );
				info->ptMinTrackSize.x = 320;
				info->ptMinTrackSize.y = 200;
			}
			break;

		case WM_ACTIVATEAPP:
			if ( app )
			{
				if ( wParam )
				{
					app->OnActivated();
				}
				else
				{
					app->OnDeactivated();
				}
			}
			break;

		case WM_POWERBROADCAST:
			switch ( wParam )
			{
				case PBT_APMQUERYSUSPEND:
					if ( !s_in_suspend && app )
						app->OnSuspending();
					s_in_suspend = true;
					return TRUE;

				case PBT_APMRESUMESUSPEND:
					if ( !s_minimized )
					{
						if ( s_in_suspend && app )
							app->OnResuming();
						s_in_suspend = false;
					}
					return TRUE;

				default:
					break;
			}
			break;

		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;

		case WM_SYSKEYDOWN:
			if ( wParam == VK_RETURN && ( lParam & 0x60000000 ) == 0x20000000 )
			{
				// Implements the classic ALT+ENTER fullscreen toggle
				if ( s_fullscreen )
				{
					SetWindowLongPtr( hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW );
					SetWindowLongPtr( hWnd, GWL_EXSTYLE, 0 );


					ShowWindow( hWnd, SW_SHOWNORMAL );

					SetWindowPos(
						hWnd,
						HWND_TOP,
						gWindowRect.left,
						gWindowRect.top,
						gWindowRect.right - gWindowRect.left,
						gWindowRect.bottom - gWindowRect.top,
						SWP_NOZORDER | SWP_FRAMECHANGED
					);
				}
				else
				{
					SetWindowLongPtr( hWnd, GWL_STYLE, WS_POPUP );
					SetWindowLongPtr( hWnd, GWL_EXSTYLE, WS_EX_TOPMOST );

					// Save rect for restore
					GetWindowRect( hWnd, &gWindowRect );

					SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED );

					ShowWindow( hWnd, SW_SHOWMAXIMIZED );
				}

				s_fullscreen = !s_fullscreen;
			}
			break;

		case WM_MENUCHAR:
			// A menu is active and the user presses a key that does not correspond
			// to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
			return MAKELRESULT( 0, MNC_CLOSE );

		default:
			break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}
