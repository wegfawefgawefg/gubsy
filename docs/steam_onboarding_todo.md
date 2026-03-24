# Steam Onboarding TODO

This is the practical checklist for getting Gubsy onto Steam without blocking normal development.

## Do Now

- Keep the company Steam account (`steam@gnk.software`, account name `gnksoftware`) as the primary admin account.
- Enable Steam Guard on that account.
- Enable 2FA on the `steam@gnk.software` mailbox.
- Set the profile display name to `G&K Software` if Steam lets you edit the profile yet.
- Keep a small internal note with:
  - Steam account name
  - recovery email
  - who controls the inbox
  - where Steam Guard codes live
- Continue repo work that does not require Steamworks:
  - shared invite/join architecture
  - non-Steam room-code flow
  - lobby/session abstractions
  - null Steam backend

## Blocked On LLC Bank Account

- Open the `G&K Software LLC` bank account.
- Make sure the legal name on the bank account matches the exact name you will use in Steamworks onboarding.
- Collect the tax info needed for company onboarding.
- Do not start Steamworks company onboarding until the bank account exists. Valve says a company signup needs a business bank account before you can proceed.

## Once Bank Account Exists

1. Sign in at `partner.steamgames.com` with the company Steam account.
2. Start Steam Direct / Steamworks onboarding as `G&K Software LLC`, not as an individual.
3. Enter the company legal name exactly as it appears on bank and tax documents.
4. Complete the tax questionnaire / verification.
5. Pay the Steam Direct fee for the app.
6. Wait for Steamworks onboarding verification to complete.

## After Steamworks Access Is Live

- Download the Steamworks SDK.
- Create the app entry for the game.
- Add personal accounts as Steamworks users with limited permissions.
- Keep the company account as the top-level admin account.
- Start the real Steam integration work:
  - SDK hookup
  - app ID setup
  - lobby/invite testing
  - achievements/cloud/workshop work later

## Timing Notes

- Valve requires a 30-day waiting period between paying the app fee and releasing the game.
- Valve also requires a publicly visible Coming Soon page for at least 2 weeks before release.
- Steam review before release usually takes 1-5 days.

## Limited Steam Account Notes

- A new Steam account is often a limited account until at least $5 USD has been spent on Steam.
- Limited accounts lose some Steam Community features.
- This does not block normal game development.
- It may affect community/profile features until the spend requirement is met.

## Sources

- Steam Direct overview: <https://partner.steamgames.com/steamdirect>
- Steam Direct fee: <https://partner.steamgames.com/doc/gettingstarted/appfee>
- Steamworks onboarding: <https://partner.steamgames.com/doc/gettingstarted/onboarding>
- Managing Steamworks users: <https://partner.steamgames.com/doc/gettingstarted/managing_users>
