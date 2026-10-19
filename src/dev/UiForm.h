#pragma once

// Single dispatch point for the two UI backends (compiled uic vs
// runtime QUiLoader). All widget code branches on `kDevUiLoader`
// with `if constexpr` — exactly one `#ifdef` lives in this file and
// none in any widget. In release the loader branch is discarded
// before instantiation, so `UiDev.cpp` (and UiTools) are never
// referenced. `UiDev.h` is always included for the declarations;
// its definitions link only into dev builds.
//
// The dev fill (`fillUi`) is fully generic over the uic struct:
// static reflection enumerates its data members, and each QObject
// member is resolved by (reflected type, reflected name) — so .ui
// files double as compile-time and runtime sources with no
// hand-maintained field list. Requires `-freflection` (see
// CMakeLists; GCC 16 P2996 snapshot: `^^` operator, access contexts,
// index-unrolled since member queries return non-constexpr vectors).
//
// Tool compat: clang-based tools (clangd, clang-tidy) don't
// implement P2996, so under them the reflection branch is replaced
// by a stub with identical signatures (`__cpp_impl_reflection`
// decides, not the build mode). Files still parse and analyze;
// a non-clang compiler without the flag fails loudly via #error
// instead of crashing on null members at runtime.

#include "UiDev.h"

#include <QVBoxLayout>
#include <QWidget>

#include <QString>

#include <utility>

#ifdef DEV_UILOADER
inline constexpr bool kDevUiLoader = true;
#else
inline constexpr bool kDevUiLoader = false;
#endif

#ifdef __cpp_impl_reflection
#include <meta>

#include <cstddef>

namespace ui_detail {

/// Member count without holding the query vector (it cannot survive
/// constant evaluation in this toolchain snapshot). Uses the
/// unprivileged context — not `current()`, which misbehaves inside
/// nested template instantiation here. Sound: uic members are public.
///
/// Counts base-class members too: uic emits `Ui::X` as an empty
/// subclass of `Ui_X`, so the members live on the base.
consteval std::size_t refl_count(std::meta::info type) {
    std::size_t n =
        std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged())
            .size();
    const auto bases = std::meta::bases_of(type, std::meta::access_context::unprivileged());
    for (std::size_t i = 0; i < bases.size(); ++i) {
        n += refl_count(std::meta::type_of(bases[i]));
    }
    return n;
}

/// I-th member across type + bases, same transient-query idiom.
consteval std::meta::info refl_at(std::meta::info type, std::size_t i) {
    const auto ctx = std::meta::access_context::unprivileged();
    const auto own = std::meta::nonstatic_data_members_of(type, ctx);
    if (i < own.size()) {
        return own[i];
    }
    const auto bases = std::meta::bases_of(type, ctx);
    i -= own.size();
    for (std::size_t b = 0; b < bases.size(); ++b) {
        const std::size_t n = refl_count(std::meta::type_of(bases[b]));
        if (i < n) {
            return refl_at(std::meta::type_of(bases[b]), i);
        }
        i -= n;
    }
    throw "member index out of range";
}

template <std::size_t I, typename Ui> void fillOne(Ui *ui, QWidget *form) {
    constexpr auto mem = refl_at(^^Ui, I);
    constexpr auto type = std::meta::type_of(mem);
    // QObject members resolve by reflected type + name; anything
    // else (spacers, plain data) is left null.
    if constexpr (std::meta::is_pointer_type(type) &&
                  std::meta::is_base_of_type(^^QObject, std::meta::remove_pointer(type))) {
        using Member = typename[:std::meta::remove_pointer(type):];
        constexpr auto name = std::meta::identifier_of(mem);
        ui->[:mem:] = form->findChild<Member *>(QString::fromUtf8(name.data(), name.size()));
    }
}

template <typename Ui, std::size_t... I>
void fillAll(Ui *ui, QWidget *form, std::index_sequence<I...>) {
    (fillOne<I>(ui, form), ...);
}

} // namespace ui_detail

template <typename Ui> void fillUi(Ui *ui, QWidget *form) {
    constexpr std::size_t n = ui_detail::refl_count(^^Ui);
    ui_detail::fillAll(ui, form, std::make_index_sequence<n>());
}
#else
#if !defined(__clang__) && defined(DEV_UILOADER)
// A real (non-clang) compiler without P2996: dev builds cannot work
// without `-freflection` — fail here, not on null members at runtime.
// (clang-based tools take the stub below.) Release builds never touch
// reflection (buildForm takes the setupUi path), so they compile
// anywhere — including cross toolchains.
#error "dev UI builds need -freflection (GCC P2996)"
#endif
// No P2996 (clang-based tools: clangd, clang-tidy): signatures only,
// so files still parse and analyze. The dev branches calling this
// analyze against valid declarations.
template <typename Ui> void fillUi(Ui *, QWidget *) {}
#endif

/// Build a content form inside `host` and fill a uic struct.
/// Release: `ui->setupUi(host)`. Dev: loader form in a marginless
/// shell, filled by reflection. Returns {ui, form}; form is null in
/// release (store it for later swaps). A missing .ui file is fatal
/// either way (the loader logs the path). Host is generic so
/// dialogs (`QDialog` setupUi overloads) build like widgets.
template <typename Ui, typename Host>
std::pair<Ui *, QWidget *> buildForm(Host *host, const char *uiFile) {
    // Value-init: setupUi overwrites every member either way, but
    // the clang tool-stub fillUi (no P2996 reflection there) would
    // otherwise leave garbage that the analyzer flags on every
    // wireUi grab. Nulls are also the honest dev-loader failure
    // state (a missing .ui is fatal below before use).
    Ui *ui = new Ui();
    if constexpr (kDevUiLoader) {
        QWidget *form = loadUiForm(QString::fromLatin1(uiFile), host);
        auto *shell = new QVBoxLayout(host);
        shell->setContentsMargins(0, 0, 0, 0);
        shell->addWidget(form);
        fillUi(ui, form);
        return {ui, form};
    } else {
        (void)uiFile;
        ui->setupUi(host);
        return {ui, nullptr};
    }
}

/// Swap a dev form for a fresh load and re-fill; true on success.
/// Release: always false (no-op).
template <typename Ui>
bool reloadForm(QWidget *host, QWidget *&formSlot, Ui *ui, const char *uiFile) {
    if constexpr (!kDevUiLoader) {
        (void)host;
        (void)formSlot;
        (void)ui;
        (void)uiFile;
        return false;
    } else {
        QWidget *form = loadUiForm(QString::fromLatin1(uiFile), host);
        if (form == nullptr) {
            return false; // logged; keep the old form
        }
        host->layout()->removeWidget(formSlot);
        formSlot->deleteLater();
        formSlot = form;
        qobject_cast<QVBoxLayout *>(host->layout())->addWidget(form);
        fillUi(ui, form);
        return true;
    }
}
