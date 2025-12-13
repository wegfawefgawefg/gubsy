#include "main_menu/menu_internal.hpp"

#include "engine/graphics.hpp"
#include "globals.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

void render_menu(int width, int height) {
    SDL_Renderer* r = gg ? gg->renderer : nullptr;
    if (!r) return;

    auto buttons = build_menu_buttons(width, height);
    ensure_focus_default(buttons);

    if (ss) {
        for (float& cd : ss->menu.audio_slider_preview_cooldown)
            cd = std::max(0.0f, cd - ss->dt);
    }

    // Draw page header and page indicator
    auto draw_text_hdr = [&](const char* s, int x, int y){
        if (!gg->ui_font) return;
        SDL_Color tc{240, 220, 80, 255};
        if (SDL_Surface* ts = TTF_RenderUTF8_Blended(gg->ui_font, s, tc)) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(r, ts);
            int tw=0, th=0; SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
            // Scale up header ~1.5x for prominence
            int sw = (int)std::lround((double)tw * 1.5);
            int sh = (int)std::lround((double)th * 1.5);
            SDL_Rect td{x, y, sw, sh}; SDL_RenderCopy(r, tt, nullptr, &td);
            SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
        }
    };
    int hdr_x = (int)std::lround(MENU_SAFE_MARGIN * static_cast<float>(width)) + 24;
    int hdr_y = (int)std::lround(MENU_SAFE_MARGIN * static_cast<float>(height)) + 16;
    const char* title = "Main Menu";
    if (ss->menu.page == SETTINGS) title = "Settings";
    else if (ss->menu.page == AUDIO) title = "Audio";
    else if (ss->menu.page == VIDEO) title = "Video";
    else if (ss->menu.page == CONTROLS) title = "Controls";
    else if (ss->menu.page == BINDS) title = "Binds";
    else if (ss->menu.page == BINDS_LOAD) title = "Load Preset";
    else if (ss->menu.page == PLAYERS) title = "Players";
    else if (ss->menu.page == OTHER) title = "Other";
    else if (ss->menu.page == MODS) title = "Mods";
    draw_text_hdr(title, hdr_x, hdr_y);
    // Page indicator on right
    if (ss->menu.page == VIDEO) {
        int total_pages = 1; // extend to >1 when paginating
        char buf[64]; std::snprintf(buf, sizeof(buf), "Page %d/%d", ss->menu.page_index + 1, 1);
        int x = width - 200; int y = hdr_y;
        draw_text_hdr(buf, x, y);
        if (total_pages > 1) {
            // Prev/Next larger buttons near top-right
            SDL_Rect prevb{width - 320, hdr_y - 4, 90, 32};
            SDL_Rect nextb{width - 120, hdr_y - 4, 90, 32};
            SDL_SetRenderDrawColor(r, 40, 45, 55, 255); SDL_RenderFillRect(r, &prevb);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &prevb);
            SDL_SetRenderDrawColor(r, 40, 45, 55, 255); SDL_RenderFillRect(r, &nextb);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &nextb);
            auto draw_small = [&](const char* s, SDL_Rect rc){ if (!gg->ui_font) return; SDL_Color c{220,220,230,255}; if (SDL_Surface* srf = TTF_RenderUTF8_Blended(gg->ui_font, s, c)) { SDL_Texture* tex = SDL_CreateTextureFromSurface(r, srf); int tw=0, th=0; SDL_QueryTexture(tex,nullptr,nullptr,&tw,&th); SDL_Rect d{rc.x + (rc.w - tw)/2, rc.y + (rc.h - th)/2, tw, th}; SDL_RenderCopy(r, tex, nullptr, &d); SDL_DestroyTexture(tex); SDL_FreeSurface(srf);} };
            draw_small("Prev", prevb);
            draw_small("Next", nextb);
        }
    } else if (ss->menu.page == BINDS) {
        // Binds keys paging header
        int total_actions = (int)BindAction::BA_COUNT;
        int per_page = 5;
        int total_pages = std::max(1, (total_actions + per_page - 1) / per_page);
        char buf[64]; std::snprintf(buf, sizeof(buf), "Page %d/%d", ss->menu.binds_keys_page + 1, total_pages);
        int x = width - 380; int y = hdr_y;
        draw_text_hdr(buf, x, y);
        if (total_pages > 1) {
            SDL_Rect prevb{width - 540, hdr_y - 4, 90, 32};
            SDL_Rect nextb{width - 120, hdr_y - 4, 90, 32};
            SDL_SetRenderDrawColor(r, 40, 45, 55, 255); SDL_RenderFillRect(r, &prevb);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &prevb);
            SDL_SetRenderDrawColor(r, 40, 45, 55, 255); SDL_RenderFillRect(r, &nextb);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &nextb);
            auto draw_small = [&](const char* s, SDL_Rect rc){ if (!gg->ui_font) return; SDL_Color c{220,220,230,255}; if (SDL_Surface* srf = TTF_RenderUTF8_Blended(gg->ui_font, s, c)) { SDL_Texture* tex = SDL_CreateTextureFromSurface(r, srf); int tw=0, th=0; SDL_QueryTexture(tex,nullptr,nullptr,&tw,&th); SDL_Rect d{rc.x + (rc.w - tw)/2, rc.y + (rc.h - th)/2, tw, th}; SDL_RenderCopy(r, tex, nullptr, &d); SDL_DestroyTexture(tex); SDL_FreeSurface(srf);} };
            draw_small("Prev", prevb);
            draw_small("Next", nextb);
        }
        // Hints: device-specific
        // defer hint drawing until draw_text is defined
    } else if (ss->menu.page == BINDS_LOAD) {
        // List paging
        int count = (int)ss->menu.binds_profiles.size();
        int per_page = 5;
        int total_pages = std::max(1, (count + per_page - 1) / per_page);
        char buf[64]; std::snprintf(buf, sizeof(buf), "Page %d/%d", ss->menu.binds_list_page + 1, total_pages);
        int x = width - 380; int y = hdr_y;
        draw_text_hdr(buf, x, y);
        if (total_pages > 1) {
            SDL_Rect prevb{width - 540, hdr_y - 4, 90, 32};
            SDL_Rect nextb{width - 120, hdr_y - 4, 90, 32};
            SDL_SetRenderDrawColor(r, 40, 45, 55, 255); SDL_RenderFillRect(r, &prevb);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &prevb);
            SDL_SetRenderDrawColor(r, 40, 45, 55, 255); SDL_RenderFillRect(r, &nextb);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &nextb);
            auto draw_small = [&](const char* s, SDL_Rect rc){ if (!gg->ui_font) return; SDL_Color c{220,220,230,255}; if (SDL_Surface* srf = TTF_RenderUTF8_Blended(gg->ui_font, s, c)) { SDL_Texture* tex = SDL_CreateTextureFromSurface(r, srf); int tw=0, th=0; SDL_QueryTexture(tex,nullptr,nullptr,&tw,&th); SDL_Rect d{rc.x + (rc.w - tw)/2, rc.y + (rc.h - th)/2, tw, th}; SDL_RenderCopy(r, tex, nullptr, &d); SDL_DestroyTexture(tex); SDL_FreeSurface(srf);} };
            draw_small("Prev", prevb);
            draw_small("Next", nextb);
        }
        // defer hint drawing until draw_text is defined
    } else if (ss->menu.page == MODS) {
        int total_pages = std::max(1, ss->menu.mods_total_pages);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Page %d/%d", ss->menu.mods_catalog_page + 1, total_pages);
        int x = width - 260; int y = hdr_y;
        draw_text_hdr(buf, x, y);
    }

    auto draw_text = [&](const char* s, int x, int y, SDL_Color col) {
        if (!gg->ui_font) return;
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, s, col)) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
            SDL_Rect td{x, y, tw, th}; SDL_RenderCopy(r, tex, nullptr, &td);
            SDL_DestroyTexture(tex); SDL_FreeSurface(surf);
        }
    };

    auto draw_label_value = [&](const SDL_Rect& rr, const std::string& label, const std::string& value, bool /*focused*/){
        SDL_Color lc{220, 220, 230, 255};
        SDL_Color vc{200, 200, 210, 255};
        int lx = rr.x + 12; int ly = rr.y + rr.h/2 - 8;
        draw_text(label.c_str(), lx, ly, lc);
        if (!value.empty() && gg->ui_font) {
            if (SDL_Surface* vs = TTF_RenderUTF8_Blended(gg->ui_font, value.c_str(), vc)) {
                SDL_Texture* vt = SDL_CreateTextureFromSurface(r, vs);
                int tw=0, th=0; SDL_QueryTexture(vt, nullptr, nullptr, &tw, &th);
                SDL_Rect vd{rr.x + rr.w - tw - 12, rr.y + rr.h/2 - th/2, tw, th};
                SDL_RenderCopy(r, vt, nullptr, &vd);
                SDL_DestroyTexture(vt); SDL_FreeSurface(vs);
            }
        }
    };

    auto draw_slider = [&](const SDL_Rect& rr, float t){
        t = std::clamp(t, 0.0f, 1.0f);
        SDL_SetRenderDrawColor(r, 60, 60, 70, 255);
        int bx = rr.x + 12, bw = rr.w - 24, by = rr.y + rr.h - 16, bh = 6;
        SDL_Rect bg{bx, by, bw, bh}; SDL_RenderFillRect(r, &bg);
        SDL_SetRenderDrawColor(r, 240, 220, 80, 220);
        int fw = (int)std::lround((double)bw * (double)t);
        SDL_Rect fg{bx, by, fw, bh}; SDL_RenderFillRect(r, &fg);
        int thw = 8, thh = 14;
        int tx = bx + fw - thw/2; int ty = by - (thh - bh)/2;
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_Rect thumb{tx, ty, thw, thh}; SDL_RenderFillRect(r, &thumb);
    };

    // Show current preset on Binds page (+ modified indicator vs snapshot)
    if (ss->menu.page == BINDS) {
        std::string preset = ss->menu.binds_current_preset.empty() ? default_input_profile_name() : ss->menu.binds_current_preset;
        bool modified = !bindings_equal(ss->input_binds, ss->menu.binds_snapshot);
        bool ro = binds_profile_is_read_only(preset);
        std::string sub = std::string("Preset: ") + preset;
        if (ro) sub += " (read-only)";
        if (modified) sub += " (modified)";
        draw_text(sub.c_str(), hdr_x, hdr_y + 36, SDL_Color{200,200,210,255});
    } else if (ss->menu.page == MODS) {
        char results_buf[64];
        std::snprintf(results_buf, sizeof(results_buf), "%d mods", ss->menu.mods_filtered_count);
        draw_text(results_buf, hdr_x, hdr_y + 36, SDL_Color{200, 200, 210, 255});
        if (ss->menu.mods_catalog_loading) {
            draw_text("Fetching catalog...", hdr_x, hdr_y + 58, SDL_Color{180, 180, 190, 255});
        } else if (!ss->menu.mods_catalog_error.empty()) {
            draw_text(ss->menu.mods_catalog_error.c_str(), hdr_x, hdr_y + 58, SDL_Color{220, 160, 160, 255});
        }
    }

    // Draw hints (after draw_text lambda defined)
    if (ss->menu.page == BINDS || ss->menu.page == BINDS_LOAD) {
        const char* hint = (ss->menu.last_input_source == 2) ? "Prev: LB  Next: RB  Back: B" : "Prev: Q  Next: E  Back: Esc";
        draw_text(hint, hdr_x, hdr_y + 58, SDL_Color{160,170,180,255});
    }

    // Draw buttons and controls
    for (auto const& b : buttons) {
        SDL_FRect pr = ndc_to_pixels(b.rect, width, height);
        bool focused = (b.id == ss->menu.focus_id);
        SDL_Color fill = focused ? SDL_Color{60, 70, 90, 255} : SDL_Color{40, 45, 55, 255};
        SDL_Color border = focused ? SDL_Color{240, 220, 80, 255} : SDL_Color{180, 180, 190, 255};
        if (!b.enabled) {
            fill = focused ? SDL_Color{50, 55, 65, 255} : SDL_Color{35, 38, 45, 255};
            border = SDL_Color{110, 110, 120, 255};
        }
        int mod_catalog_idx = -1;
        if (ss->menu.page == MODS && b.id >= 900 && b.id < 900 + kModsPerPage) {
            int rel = b.id - 900;
            int abs_idx = ss->menu.mods_catalog_page * kModsPerPage + rel;
            if (abs_idx >= 0 && abs_idx < (int)ss->menu.mods_visible_indices.size())
                mod_catalog_idx = ss->menu.mods_visible_indices[(size_t)abs_idx];
            if (mod_catalog_idx >= 0) {
                const auto& catalog = menu_mod_catalog();
                if (mod_catalog_idx < (int)catalog.size()) {
                    const auto& entry = catalog[(size_t)mod_catalog_idx];
                    if (entry.installed) {
                        fill = focused ? SDL_Color{70, 90, 70, 255} : SDL_Color{45, 55, 45, 255};
                    }
                }
            }
        }
        SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
        SDL_Rect rr{(int)std::floor(pr.x), (int)std::floor(pr.y), (int)std::ceil(pr.w), (int)std::ceil(pr.h)};
        SDL_RenderFillRect(r, &rr);
        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(r, &rr);
        if (b.kind == ButtonKind::Button) {
            if (ss->menu.page == BINDS && b.id >= 700 && b.id <= 704) {
                static const BindAction order[] = {
                    BA_LEFT, BA_RIGHT, BA_UP, BA_DOWN,
                    BA_USE_LEFT, BA_USE_RIGHT, BA_USE_UP, BA_USE_DOWN, BA_USE_CENTER,
                    BA_PICK_UP, BA_DROP, BA_RELOAD, BA_DASH
                };
                int per_page = 5;
                int idx = ss->menu.binds_keys_page * per_page + (b.id - 700);
                if (idx >= 0 && idx < (int)(sizeof(order)/sizeof(order[0]))) {
                    BindAction a = order[idx];
                    std::string label = bind_label(a);
                    SDL_Scancode sc = bind_get(ss->input_binds, a);
                    draw_label_value(rr, label, std::string(SDL_GetScancodeName(sc)), focused);
                }
            } else if (ss->menu.page == BINDS_LOAD) {
                const auto& profiles = ss->menu.binds_profiles;
                int per_page = 5;
                if (b.id >= 720 && b.id <= 724) {
                    int idx = ss->menu.binds_list_page * per_page + (b.id - 720);
                    if (idx >= 0 && idx < (int)profiles.size()) {
                        const auto& info = profiles[static_cast<size_t>(idx)];
                        std::string label = info.name;
                        if (info.name == ss->menu.binds_current_preset) label += " (Active)";
                        if (info.read_only) label += " (Default)";
                        draw_label_value(rr, label, std::string(), focused);
                    }
                } else if (b.id >= 730 && b.id <= 734) {
                    draw_label_value(rr, "Rename", std::string(), focused);
                } else if (b.id >= 740 && b.id <= 744) {
                    draw_label_value(rr, "Delete", std::string(), focused);
                } else {
                    draw_label_value(rr, b.label, std::string(), focused);
                }
            } else if (ss->menu.page == MODS) {
                if (b.id == 950) {
                    std::string query = ss->menu.mods_search_query.empty()
                                            ? "Type to filter mods..."
                                            : ss->menu.mods_search_query;
                    SDL_Color label_col{220, 220, 230, 255};
                    SDL_Color lc = ss->menu.mods_search_query.empty()
                                       ? SDL_Color{150, 150, 170, 255}
                                       : SDL_Color{220, 220, 230, 255};
                    draw_text("Search", rr.x + 12, rr.y + 10, label_col);
                    draw_text(query.c_str(), rr.x + 12, rr.y + rr.h / 2, lc);
                } else if (b.id == 951 || b.id == 952) {
                    draw_label_value(rr, b.label, std::string(), focused);
                } else if (mod_catalog_idx >= 0) {
                    const auto& catalog = menu_mod_catalog();
                    if (mod_catalog_idx >= (int)catalog.size())
                        continue;
                    const auto& entry = catalog[(size_t)mod_catalog_idx];
                    SDL_Color title_col{240, 220, 80, 255};
                    SDL_Color meta_col{190, 190, 200, 255};
                    SDL_Color desc_col{200, 200, 210, 255};
                    draw_text(entry.title.c_str(), rr.x + 12, rr.y + 10, title_col);
                    char meta[128];
                    std::snprintf(meta, sizeof(meta), "v%s  •  %s", entry.version.c_str(), entry.author.c_str());
                    draw_text(meta, rr.x + 12, rr.y + 34, meta_col);
                    std::string summary = entry.summary;
                    if (summary.size() > 140) {
                        summary.resize(137);
                        summary += "...";
                    }
                    draw_text(summary.c_str(), rr.x + 12, rr.y + 54, desc_col);
                    std::string deps = entry.dependencies.empty() ? "Standalone" : "Requires: ";
                    if (!entry.dependencies.empty()) {
                        for (size_t i = 0; i < entry.dependencies.size(); ++i) {
                            if (i > 0) deps += ", ";
                            deps += entry.dependencies[i];
                        }
                    }
                    draw_text(deps.c_str(), rr.x + 12, rr.y + rr.h - 32, SDL_Color{170, 170, 190, 255});
                    SDL_Rect pill{rr.x + rr.w - 160, rr.y + rr.h - 40, 140, 26};
                    bool installed = entry.installed;
                    SDL_Color pill_fill{60, 70, 90, 255};
                    SDL_Color pill_border{180, 180, 190, 255};
                    if (entry.required) {
                        pill_fill = SDL_Color{80, 80, 100, 255};
                    } else if (entry.installing || entry.uninstalling) {
                        pill_fill = SDL_Color{90, 90, 60, 255};
                        pill_border = SDL_Color{230, 230, 160, 255};
                    } else if (installed) {
                        pill_fill = focused ? SDL_Color{90, 110, 90, 255} : SDL_Color{70, 90, 70, 255};
                        pill_border = SDL_Color{200, 220, 200, 255};
                    }
                    SDL_SetRenderDrawColor(r, pill_fill.r, pill_fill.g, pill_fill.b, pill_fill.a);
                    SDL_RenderFillRect(r, &pill);
                    SDL_SetRenderDrawColor(r, pill_border.r, pill_border.g, pill_border.b, pill_border.a);
                    SDL_RenderDrawRect(r, &pill);
                    std::string status = "Install";
                    if (entry.required)
                        status = "Core";
                    else if (entry.installing)
                        status = "Installing...";
                    else if (entry.uninstalling)
                        status = "Removing...";
                    else if (installed)
                        status = "Uninstall";
                    draw_text(status.c_str(), pill.x + 12, pill.y + 5, SDL_Color{230, 230, 240, 255});
                    if (!entry.status_text.empty()) {
                        draw_text(entry.status_text.c_str(), rr.x + 12, rr.y + rr.h - 12,
                                  SDL_Color{180, 200, 200, 255});
                    }
                } else if (b.id == 998) {
                    draw_label_value(rr, b.label, std::string(), focused);
                } else {
                    draw_label_value(rr, b.label, std::string(), focused);
                }
            } else if (b.id == 498) {
                // Apply button highlight when enabled
                draw_label_value(rr, b.label, std::string(), focused);
            } else {
                draw_label_value(rr, b.label, std::string(), focused);
            }
        } else if (b.kind == ButtonKind::Toggle) {
            bool on = (b.value >= 0.5f);
            draw_label_value(rr, b.label, on ? std::string("On") : std::string("Off"), focused);
        } else if (b.kind == ButtonKind::Slider) {
            char buf[64] = {0};
            if (b.id == 501 || b.id == 502) {
                double minv = 0.10, maxv = 10.0;
                double val = minv + (maxv - minv) * (double)b.value;
                std::snprintf(buf, sizeof(buf), "%.3f", val);
            } else {
                double dv = static_cast<double>(b.value) * 100.0;
                int pct = (int)std::lround(dv);
                std::snprintf(buf, sizeof(buf), "%d%%", pct);
            }
            draw_label_value(rr, b.label, std::string(buf), focused);
            draw_slider(rr, b.value);
        } else if (b.kind == ButtonKind::OptionCycle) {
            std::string val;
            if (b.id == 400) {
                static const char* kRes[] = {"1280x720", "1600x900", "1920x1080", "2560x1440"};
                int idx = std::clamp(ss->menu.video_res_index, 0, 3);
                val = kRes[idx];
            } else if (b.id == 405) {
                static const char* kWin[] = {"1280x720", "1600x900", "1920x1080", "2560x1440"};
                int idx = std::clamp(ss->menu.window_size_index, 0, 3);
                val = kWin[idx];
            } else if (b.id == 401) {
                static const char* kModes[] = {"Windowed", "Borderless", "Fullscreen"};
                int idx = std::clamp(ss->menu.window_mode_index, 0, 2);
                val = kModes[idx];
            } else if (b.id == 403) {
                static const char* kFps[] = {"Off", "30", "60", "120", "144", "240"};
                int idx = std::clamp(ss->menu.frame_limit_index, 0, 5);
                val = kFps[idx];
            }
            // Draw label, value, and left/right arrows
            SDL_Color lc{220, 220, 230, 255};
            int lx = rr.x + 12; int ly = rr.y + rr.h/2 - 8; draw_text(b.label.c_str(), lx, ly, lc);
            // Arrow buttons (larger) with padding from right edge
            int padR = 16;
            int aw = std::min(36, rr.w / 6);
            int ah = std::min(28, rr.h - 8);
            SDL_Rect leftA{rr.x + rr.w - (aw*2 + padR), rr.y + rr.h/2 - ah/2, aw, ah};
            SDL_Rect rightA{rr.x + rr.w - (aw + padR/2), rr.y + rr.h/2 - ah/2, aw, ah};
            SDL_SetRenderDrawColor(r, 60, 60, 70, 255); SDL_RenderFillRect(r, &leftA); SDL_RenderFillRect(r, &rightA);
            SDL_SetRenderDrawColor(r, 180, 180, 190, 255); SDL_RenderDrawRect(r, &leftA); SDL_RenderDrawRect(r, &rightA);
            draw_text("<", leftA.x + 7, leftA.y + 2, lc);
            draw_text(">", rightA.x + 7, rightA.y + 2, lc);
            // Value right-aligned, before arrows
            if (gg->ui_font) {
                SDL_Color vc{200, 200, 210, 255};
                if (SDL_Surface* vs = TTF_RenderUTF8_Blended(gg->ui_font, val.c_str(), vc)) {
                    SDL_Texture* vt = SDL_CreateTextureFromSurface(r, vs);
                    int tw=0, th=0; SDL_QueryTexture(vt, nullptr, nullptr, &tw, &th);
                    SDL_Rect vd{leftA.x - 16 - tw, rr.y + rr.h/2 - th/2, tw, th};
                    SDL_RenderCopy(r, vt, nullptr, &vd);
                    SDL_DestroyTexture(vt); SDL_FreeSurface(vs);
                }
            }
        }
    }

    // Binds capture overlay
    if (ss->menu.capture_action_id >= 0) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 10, 10, 14, 180);
        SDL_Rect full{0,0,width,height};
        SDL_RenderFillRect(r, &full);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        const char* msg = "Press a key to bind (Esc to cancel)";
        auto draw_center = [&](const char* s){
            if (!gg->ui_font) return;
            SDL_Color c{240, 220, 80, 255};
            if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, s, c)) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                SDL_Rect td{width/2 - tw/2, height/2 - th/2, tw, th};
                SDL_RenderCopy(r, tex, nullptr, &td);
                SDL_DestroyTexture(tex); SDL_FreeSurface(surf);
            }
        };
        draw_center(msg);
    }

    // Toast overlay for binds actions
    if (ss->menu.binds_toast_timer > 0.0f) {
        ss->menu.binds_toast_timer = std::max(0.0f, ss->menu.binds_toast_timer - ss->dt);
        if (!ss->menu.binds_toast.empty() && gg && gg->ui_font) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_Color c{240, 220, 80, 255};
            const std::string& s = ss->menu.binds_toast;
            if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, s.c_str(), c)) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                float t = 1.0f - (ss->menu.binds_toast_timer / 1.2f);
                int yoff = (int)std::lround(-40.0f * t);
                SDL_Rect td{width/2 - tw/2, height/2 - th/2 + yoff, tw, th};
                SDL_RenderCopy(r, tex, nullptr, &td);
                SDL_DestroyTexture(tex); SDL_FreeSurface(surf);
            }
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
    }

    if (menu_is_text_input_active() && gg && gg->ui_font) {
        SDL_Rect modal{width / 2 - 220, height / 2 - 90, 440, 180};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 15, 15, 25, 235);
        SDL_RenderFillRect(r, &modal);
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_RenderDrawRect(r, &modal);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_Color text_col{240, 240, 250, 255};
        const char* modal_title = "Preset Name";
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, modal_title, text_col)) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst{modal.x + (modal.w - tw)/2, modal.y + 16, tw, th};
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
        SDL_Rect input_box{modal.x + 24, modal.y + 60, modal.w - 48, 48};
        SDL_SetRenderDrawColor(r, 50, 55, 70, 255);
        SDL_RenderFillRect(r, &input_box);
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_RenderDrawRect(r, &input_box);
        std::string buffer = ss->menu.binds_text_input_buffer;
        if ((int)buffer.size() < ss->menu.binds_text_input_limit)
            buffer.push_back('_');
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, buffer.c_str(), text_col)) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst{input_box.x + 12, input_box.y + input_box.h/2 - th/2, tw, th};
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
        char count_buf[32];
        std::snprintf(count_buf, sizeof(count_buf), "%d/%d", (int)ss->menu.binds_text_input_buffer.size(), ss->menu.binds_text_input_limit);
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, count_buf, SDL_Color{180,180,190,255})) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst{input_box.x + input_box.w - tw - 8, input_box.y + input_box.h + 8, tw, th};
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
        if (!ss->menu.binds_text_input_error.empty() && ss->menu.binds_text_input_error_timer > 0.0f) {
            ss->menu.binds_text_input_error_timer = std::max(0.0f, ss->menu.binds_text_input_error_timer - ss->dt);
            SDL_Color err_col{255, 120, 120, 255};
            if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, ss->menu.binds_text_input_error.c_str(), err_col)) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                SDL_Rect dst{modal.x + (modal.w - tw)/2, input_box.y + input_box.h + 28, tw, th};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
                SDL_FreeSurface(surf);
            }
            if (ss->menu.binds_text_input_error_timer <= 0.0f)
                ss->menu.binds_text_input_error.clear();
        } else if (ss->menu.binds_text_input_error_timer <= 0.0f) {
            ss->menu.binds_text_input_error.clear();
        }
        const char* hint = "Enter = Confirm   Esc = Cancel";
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, hint, SDL_Color{200,200,210,255})) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst{modal.x + (modal.w - tw)/2, modal.y + modal.h - th - 12, tw, th};
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }
}
