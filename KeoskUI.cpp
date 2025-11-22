#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <iostream>

//Data structures
struct Item { 
    int id; 
    std::string name; 
    int quantity=0; 
    double price=0.0; 
};
struct CartLine { 
    int id; 
    std::string name; 
    int qty=0; 
    double unitPrice=0.0; 
};
enum class PaymentMethod { ApplePay, GooglePay, EMVChip, Magstripe };
enum class PaymentOutcome { Approved, Declined, Timeout, Random };

//Global state
static std::vector<Item> g_inventory;
static std::vector<CartLine> g_cart;
static PaymentMethod g_method = PaymentMethod::ApplePay;
static PaymentOutcome g_outcome = PaymentOutcome::Random;
static bool g_bundleChecked = false;
static HFONT g_font = nullptr;
static HWND hGrpInv, hLbInv, hGrpCart, hLbCart, hLblQty, hEditQty, hBtnAdd, hBtnRemove;
static HWND hChkBundle, hLblTotal, hBtnCheckout, hBtnCancel, hLblMethod;
static HWND hBtnApple, hBtnGoogle, hBtnEmv, hBtnMag, hRadAppr, hRadDecl, hRadTimeout, hRadRandom;

//Timestamp helper
static std::string nowTimestamp(){
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

//Log UI interactions to console and file
static void logInteraction(const std::string& msg){
    std::cout << "[" << nowTimestamp() << "] " << msg << std::endl;
    std::ofstream log("interactions.log", std::ios::app);
    if(log.good()) log << "[" << nowTimestamp() << "] " << msg << "\n";
}

//Log purchase transactions
static void logPurchase(const std::vector<CartLine>& cart, double total, PaymentMethod pm, bool success){
    std::ofstream log("purchases.log", std::ios::app);
    if(!log.good()) return;
    log << nowTimestamp() << ",";
    log << (success?"SUCCESS":"FAILED") << ",";
    const char* method = "ApplePay";
    if(pm==PaymentMethod::GooglePay) method="GooglePay";
    else if(pm==PaymentMethod::EMVChip) method="EMVChip";
    else if(pm==PaymentMethod::Magstripe) method="Magstripe";
    log << method << ",$" << std::fixed << std::setprecision(2) << total << ",\"";
    for(size_t i=0; i<cart.size(); ++i){
        log << cart[i].name << "x" << cart[i].qty;
        if(i+1 < cart.size()) log << ";";
    }
    log << "\"\n";
}

//Save inventory to CSV
static void saveInventory(){
    std::ofstream out("inventory.csv", std::ios::trunc);
    if(!out.good()){ logInteraction("ERROR: Failed to save inventory"); return; }
    out << "id,name,quantity,price\n";
    for(auto& it : g_inventory){
        out << it.id << "," << it.name << "," << it.quantity << "," 
            << std::fixed << std::setprecision(2) << it.price << "\n";
    }
    logInteraction("Inventory saved to inventory.csv");
}

//Load inventory from CSV or create default
static void loadInventory(){
    std::ifstream in("inventory.csv");
    if(!in.good()){
        logInteraction("inventory.csv not found - creating default inventory");
        g_inventory = {
            {1, "Water Bottle 16oz", 50, 1.50},
            {2, "Trail Mix 3oz", 35, 2.99},
            {3, "AA Batteries (4 pack)", 20, 5.49},
            {4, "Trail-Assist Bundle", 15, 9.99},
            {5, "Repair Kit", 10, 7.25},
            {6, "Tent Clip Set", 8, 4.99}
        };
        saveInventory();
        return;
    }
    g_inventory.clear();
    std::string line;
    std::getline(in, line);
    while(std::getline(in, line)){
        if(line.empty()) continue;
        std::stringstream ss(line);
        std::string id, name, qty, price;
        std::getline(ss, id, ',');
        std::getline(ss, name, ',');
        std::getline(ss, qty, ',');
        std::getline(ss, price, ',');
        Item it;
        it.id = std::stoi(id);
        it.name = name;
        it.quantity = std::stoi(qty);
        it.price = std::stod(price);
        g_inventory.push_back(it);
    }
    logInteraction("Inventory loaded from inventory.csv - " + std::to_string(g_inventory.size()) + " items");
}

//String conversion helper
static std::wstring s2ws(const std::string& s){
    if(s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

//Message box helper
static void showMsg(HWND h, const wchar_t* t, const wchar_t* c = L"Info", UINT icon = MB_ICONINFORMATION){
    MessageBoxW(h, t, c, MB_OK | icon);
}

//Find item by ID
static Item* findItem(int id){
    for(auto& it : g_inventory) if(it.id == id) return &it;
    return nullptr;
}

static const Item* findItemC(int id){
    for(auto& it : g_inventory) if(it.id == id) return &it;
    return nullptr;
}

//Find cart line by item ID
static CartLine* findCart(int id){
    for(auto& cl : g_cart) if(cl.id == id) return &cl;
    return nullptr;
}

//Add item to cart
static void addToCart(const Item& it, int qty){
    if(qty <= 0) return;
    if(auto* cl = findCart(it.id)){
        cl->qty += qty;
        logInteraction("Updated cart: " + it.name + " quantity increased to " + std::to_string(cl->qty));
    }else{
        g_cart.push_back({it.id, it.name, qty, it.price});
        logInteraction("Added to cart: " + it.name + " x" + std::to_string(qty) + " @ $" + std::to_string(it.price));
    }
}

//Remove item from cart
static void removeFromCart(int id, int qty){
    for(size_t i = 0; i < g_cart.size(); ++i){
        if(g_cart[i].id == id){
            std::string itemName = g_cart[i].name;
            if(qty <= 0 || qty >= g_cart[i].qty){
                g_cart.erase(g_cart.begin() + i);
                logInteraction("Removed from cart: " + itemName);
            }else{
                g_cart[i].qty -= qty;
                logInteraction("Decreased cart quantity: " + itemName + " now " + std::to_string(g_cart[i].qty));
            }
            return;
        }
    }
}

//Calculate cart total
static double calcTotal(){
    double t = 0.0;
    for(auto& cl : g_cart) t += cl.qty * cl.unitPrice;
    return t;
}

//Validate stock availability
static bool validateStock(std::string& err){
    for(auto& cl : g_cart){
        auto* it = findItemC(cl.id);
        if(!it){
            err = "Item not found: ID " + std::to_string(cl.id);
            return false;
        }
        if(it->quantity < cl.qty){
            err = "Insufficient stock for " + it->name;
            return false;
        }
    }
    return true;
}

//Commit sale and update inventory
static void commitSale(){
    for(auto& cl : g_cart){
        auto* it = findItem(cl.id);
        if(it){
            it->quantity -= cl.qty;
            logInteraction("Inventory updated: " + it->name + " reduced by " + std::to_string(cl.qty) + 
                          " (remaining: " + std::to_string(it->quantity) + ")");
        }
    }
    saveInventory();
}

//Ensure bundle in/out of cart based on checkbox
static void ensureBundleInCart(bool want){
    auto* cl = findCart(4);
    if(want){
        if(!cl){
            auto* it = findItemC(4);
            if(it){
                g_cart.push_back({it->id, it->name, 1, it->price});
                logInteraction("Bundle added to cart automatically");
            }
        }else{
            cl->qty = 1;
        }
    }else{
        if(cl){
            removeFromCart(4, cl->qty);
            logInteraction("Bundle removed from cart");
        }
    }
}

//Refresh inventory list UI
static void refreshInventoryList(){
    SendMessageW(hLbInv, LB_RESETCONTENT, 0, 0);
    for(auto& it : g_inventory){
        std::ostringstream os;
        os << it.name << " - $" << std::fixed << std::setprecision(2) << it.price 
           << " (Stock: " << it.quantity << ")";
        SendMessageW(hLbInv, LB_ADDSTRING, 0, (LPARAM)s2ws(os.str()).c_str());
    }
}

//Refresh cart list UI
static void refreshCartList(){
    SendMessageW(hLbCart, LB_RESETCONTENT, 0, 0);
    for(auto& cl : g_cart){
        std::ostringstream os;
        os << cl.name << " x" << cl.qty << " @ $" << std::fixed << std::setprecision(2) 
           << cl.unitPrice << " = $" << std::fixed << std::setprecision(2) << (cl.unitPrice * cl.qty);
        SendMessageW(hLbCart, LB_ADDSTRING, 0, (LPARAM)s2ws(os.str()).c_str());
    }
}

//Update total label
static void updateTotalLabel(){
    std::wstringstream ws;
    ws << L"Total: $" << std::fixed << std::setprecision(2) << calcTotal();
    SetWindowTextW(hLblTotal, ws.str().c_str());
}

//Update payment method label
static void updateMethodLabel(){
    const wchar_t* w = L"Method: Apple Pay";
    if(g_method == PaymentMethod::GooglePay) w = L"Method: Google Pay";
    else if(g_method == PaymentMethod::EMVChip) w = L"Method: EMV Chip";
    else if(g_method == PaymentMethod::Magstripe) w = L"Method: Magstripe";
    SetWindowTextW(hLblMethod, w);
}

//Simulate payment outcome
static PaymentOutcome simulate(PaymentOutcome chosen){
    if(chosen != PaymentOutcome::Random) return chosen;
    int r = rand() % 100;
    if(r < 75) return PaymentOutcome::Approved;
    if(r < 95) return PaymentOutcome::Declined;
    return PaymentOutcome::Timeout;
}

//Layout controls in window
static void doLayout(HWND hWnd){
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    int margin = 10, col = (W - margin * 4) / 3;
    int leftX = margin, midX = leftX + col + margin, rightX = midX + col + margin;
    int top = margin, bottomPanelH = 160, midY = top + 10, y = H - bottomPanelH + 5;
    
    MoveWindow(hGrpInv, leftX, top, col, H - bottomPanelH - margin, TRUE);
    MoveWindow(hLbInv, leftX + 10, top + 25, col - 20, H - bottomPanelH - 45, TRUE);
    MoveWindow(hGrpCart, rightX, top, col, H - bottomPanelH - margin, TRUE);
    MoveWindow(hLbCart, rightX + 10, top + 25, col - 20, H - bottomPanelH - 95, TRUE);
    MoveWindow(hLblTotal, rightX + 10, H - bottomPanelH - 60, col - 20, 24, TRUE);
    MoveWindow(hBtnRemove, rightX + 10, H - bottomPanelH - 30, 120, 26, TRUE);
    MoveWindow(hLblQty, midX, midY, col, 20, TRUE);
    MoveWindow(hEditQty, midX + 10, midY + 22, col - 20, 26, TRUE);
    MoveWindow(hBtnAdd, midX + 10, midY + 54, col - 20, 28, TRUE);
    MoveWindow(hChkBundle, midX + 10, midY + 90, col - 20, 22, TRUE);
    MoveWindow(hLblMethod, leftX, y, col, 24, TRUE);
    MoveWindow(hBtnApple, leftX, y + 30, 120, 28, TRUE);
    MoveWindow(hBtnGoogle, leftX + 130, y + 30, 120, 28, TRUE);
    MoveWindow(hBtnEmv, leftX, y + 64, 120, 28, TRUE);
    MoveWindow(hBtnMag, leftX + 130, y + 64, 120, 28, TRUE);
    MoveWindow(hRadAppr, midX, y + 10, col - 20, 22, TRUE);
    MoveWindow(hRadDecl, midX, y + 36, col - 20, 22, TRUE);
    MoveWindow(hRadTimeout, midX, y + 62, col - 20, 22, TRUE);
    MoveWindow(hRadRandom, midX, y + 88, col - 20, 22, TRUE);
    MoveWindow(hBtnCheckout, rightX, y + 10, col, 36, TRUE);
    MoveWindow(hBtnCancel, rightX, y + 54, col, 36, TRUE);
}

//Create window control helper
static HWND make(HWND parent, const wchar_t* cls, const wchar_t* txt, DWORD style){
    HWND h = CreateWindowExW(0, cls, txt, style, 0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if(h && g_font) SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    return h;
}

//Create all UI controls
static void createControls(HWND hWnd){
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    
    hGrpInv = make(hWnd, L"BUTTON", L"Inventory", WS_CHILD | WS_VISIBLE | BS_GROUPBOX);
    hGrpCart = make(hWnd, L"BUTTON", L"Cart", WS_CHILD | WS_VISIBLE | BS_GROUPBOX);
    hLbInv = make(hWnd, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL);
    hLbCart = make(hWnd, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL);
    hLblQty = make(hWnd, L"STATIC", L"Quantity", WS_CHILD | WS_VISIBLE);
    hEditQty = make(hWnd, L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER);
    SendMessageW(hEditQty, EM_SETLIMITTEXT, 3, 0);
    hBtnAdd = make(hWnd, L"BUTTON", L"Add to Cart", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
    hBtnRemove = make(hWnd, L"BUTTON", L"Remove Selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
    hChkBundle = make(hWnd, L"BUTTON", L"Trail-Assist Bundle ($9.99)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX);
    hLblTotal = make(hWnd, L"STATIC", L"Total: $0.00", WS_CHILD | WS_VISIBLE);
    hLblMethod = make(hWnd, L"STATIC", L"Method: Apple Pay", WS_CHILD | WS_VISIBLE);
    hBtnApple = make(hWnd, L"BUTTON", L"Apple Pay", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
    hBtnGoogle = make(hWnd, L"BUTTON", L"Google Pay", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
    hBtnEmv = make(hWnd, L"BUTTON", L"EMV Chip", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
    hBtnMag = make(hWnd, L"BUTTON", L"Magstripe", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
    hRadAppr = make(hWnd, L"BUTTON", L"Approved", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP);
    hRadDecl = make(hWnd, L"BUTTON", L"Declined", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON);
    hRadTimeout = make(hWnd, L"BUTTON", L"Timeout", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON);
    hRadRandom = make(hWnd, L"BUTTON", L"Random", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON);
    SendMessageW(hRadRandom, BM_SETCHECK, BST_CHECKED, 0);
    hBtnCheckout = make(hWnd, L"BUTTON", L"Checkout", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON);
    hBtnCancel = make(hWnd, L"BUTTON", L"Cancel (Reset)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON);
}

//Show purchase summary dialog
static void showSummary(HWND hWnd){
    std::wstringstream ss;
    ss << L"Order Summary\n\n";
    for(auto& cl : g_cart){
        const Item* it = findItemC(cl.id);
        int remain = it ? it->quantity : -1;
        ss << s2ws(cl.name) << L" x" << cl.qty << L"   Remaining: " << remain << L"\n";
    }
    ss << L"\nTotal: $" << std::fixed << std::setprecision(2) << calcTotal() << L"\n";
    const wchar_t* m = L"Apple Pay";
    if(g_method == PaymentMethod::GooglePay) m = L"Google Pay";
    else if(g_method == PaymentMethod::EMVChip) m = L"EMV Chip";
    else if(g_method == PaymentMethod::Magstripe) m = L"Magstripe";
    ss << L"Payment: " << m << L"\n";
    MessageBoxW(hWnd, ss.str().c_str(), L"Purchase Complete", MB_OK | MB_ICONINFORMATION);
}

//Reset UI to initial state
static void resetUI(){
    g_cart.clear();
    g_bundleChecked = false;
    SendMessageW(hChkBundle, BM_SETCHECK, BST_UNCHECKED, 0);
    g_method = PaymentMethod::ApplePay;
    g_outcome = PaymentOutcome::Random;
    SendMessageW(hRadRandom, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(hRadAppr, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(hRadDecl, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(hRadTimeout, BM_SETCHECK, BST_UNCHECKED, 0);
    updateMethodLabel();
    refreshCartList();
    updateTotalLabel();
    logInteraction("UI Reset - Cart cleared");
}

//Main window procedure
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_CREATE:
            AllocConsole();
            FILE* fp;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            srand((unsigned)time(nullptr));
            logInteraction("=== Kiosk Application Started ===");
            loadInventory();
            createControls(hWnd);
            refreshInventoryList();
            refreshCartList();
            updateTotalLabel();
            doLayout(hWnd);
            return 0;
        case WM_SIZE:
            doLayout(hWnd);
            return 0;
        case WM_COMMAND:{
            HWND hCtrl = (HWND)lParam;
            if(hCtrl == hBtnApple){ g_method = PaymentMethod::ApplePay; updateMethodLabel(); logInteraction("Payment method changed to Apple Pay"); }
            else if(hCtrl == hBtnGoogle){ g_method = PaymentMethod::GooglePay; updateMethodLabel(); logInteraction("Payment method changed to Google Pay"); }
            else if(hCtrl == hBtnEmv){ g_method = PaymentMethod::EMVChip; updateMethodLabel(); logInteraction("Payment method changed to EMV Chip"); }
            else if(hCtrl == hBtnMag){ g_method = PaymentMethod::Magstripe; updateMethodLabel(); logInteraction("Payment method changed to Magstripe"); }
            else if(hCtrl == hRadAppr){ g_outcome = PaymentOutcome::Approved; logInteraction("Simulation mode: Approved"); }
            else if(hCtrl == hRadDecl){ g_outcome = PaymentOutcome::Declined; logInteraction("Simulation mode: Declined"); }
            else if(hCtrl == hRadTimeout){ g_outcome = PaymentOutcome::Timeout; logInteraction("Simulation mode: Timeout"); }
            else if(hCtrl == hRadRandom){ g_outcome = PaymentOutcome::Random; logInteraction("Simulation mode: Random"); }
            else if(hCtrl == hChkBundle){
                g_bundleChecked = (SendMessageW(hChkBundle, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ensureBundleInCart(g_bundleChecked);
                refreshCartList();
                updateTotalLabel();
            }
            else if(hCtrl == hBtnAdd){
                int sel = (int)SendMessageW(hLbInv, LB_GETCURSEL, 0, 0);
                if(sel == LB_ERR){ showMsg(hWnd, L"Select an item in Inventory.", L"Add to Cart", MB_ICONWARNING); return 0; }
                if(sel < 0 || sel >= (int)g_inventory.size()){ showMsg(hWnd, L"Invalid selection.", L"Add", MB_ICONERROR); return 0; }
                Item* it = &g_inventory[sel];
                wchar_t qbuf[16];
                GetWindowTextW(hEditQty, qbuf, 16);
                int q = _wtoi(qbuf);
                if(q <= 0) q = 1;
                addToCart(*it, q);
                refreshCartList();
                updateTotalLabel();
            }
            else if(hCtrl == hBtnRemove){
                int sel = (int)SendMessageW(hLbCart, LB_GETCURSEL, 0, 0);
                if(sel == LB_ERR) return 0;
                if(sel >= 0 && sel < (int)g_cart.size()){
                    int itemId = g_cart[sel].id;
                    removeFromCart(itemId, 0);
                    refreshCartList();
                    updateTotalLabel();
                }
            }
            else if(hCtrl == hBtnCheckout){
                if(g_cart.empty()){ showMsg(hWnd, L"Cart is empty.", L"Checkout", MB_ICONWARNING); logInteraction("Checkout attempted with empty cart"); return 0; }
                ensureBundleInCart(g_bundleChecked);
                std::string err;
                if(!validateStock(err)){ showMsg(hWnd, s2ws("Stock check failed: " + err).c_str(), L"Checkout", MB_ICONERROR); logInteraction("Stock validation failed: " + err); return 0; }
                logInteraction("Processing payment - Total: $" + std::to_string(calcTotal()));
                PaymentOutcome res = simulate(g_outcome);
                if(res != PaymentOutcome::Approved){
                    logInteraction("Payment " + std::string(res == PaymentOutcome::Declined ? "DECLINED" : "TIMEOUT"));
                    int mb = MessageBoxW(hWnd, res == PaymentOutcome::Declined ? L"Payment Declined.\nRetry?" : L"Payment Timeout.\nRetry?", L"Payment", MB_RETRYCANCEL | MB_ICONWARNING);
                    if(mb == IDRETRY){
                        logInteraction("Retrying payment...");
                        res = simulate(g_outcome);
                    }else{
                        logPurchase(g_cart, calcTotal(), g_method, false);
                        resetUI();
                        return 0;
                    }
                }
                if(res != PaymentOutcome::Approved){
                    logInteraction("Payment failed after retry");
                    logPurchase(g_cart, calcTotal(), g_method, false);
                    showMsg(hWnd, L"Payment could not be completed. Returning to main screen.", L"Payment", MB_ICONERROR);
                    resetUI();
                    return 0;
                }
                logInteraction("Payment APPROVED");
                logPurchase(g_cart, calcTotal(), g_method, true);
                commitSale();
                refreshInventoryList();
                showSummary(hWnd);
                resetUI();
            }
            else if(hCtrl == hBtnCancel){
                logInteraction("Cancel button pressed");
                resetUI();
            }
            return 0;
        }
        case WM_DESTROY:
            logInteraction("=== Kiosk Application Closing ===");
            if(g_font){ DeleteObject(g_font); g_font = nullptr; }
            FreeConsole();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

//Application entry point
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmd){
    WNDCLASSW wc{};
    wc.hInstance = hInst;
    wc.lpszClassName = L"KioskUserformWin32";
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    
    HWND hWnd = CreateWindowExW(0, L"KioskUserformWin32", L"TrailHub Kiosk", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 640, nullptr, nullptr, hInst, nullptr);
    
    MSG msg;
    while(GetMessageW(&msg, nullptr, 0, 0)){
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
