# How Freeciv upstream handles contributed changes

Research note, read-only survey taken 2026-11-01 of the Freeciv Redmine
(https://redmine.freeciv.org, project `freeciv`, id 1) and the GitHub mirror
(freeciv/freeciv). Purpose: file tickets and PRs for the Deep-AI pathfinder /
advisor work in a shape upstream will actually take.

Method: anonymous Redmine REST reads (`/issues.json`, `/issues/<id>.json?include=
journals,attachments,relations`), `gh pr list/view` for GitHub, and the upstream
git history already in this checkout (`upstream/main`, stable branches). Raw JSON
is in the session scratchpad (`customs/`). Sample: ~70 tickets read in full
(about 30 by outside contributors, the rest by the maintainer), 25 PRs read with
comments and commits, plus the trailer frequency over the last 2000 upstream
commits. Quotes are verbatim; ticket numbers are Redmine `#NNNN`, PRs are
`PR #NN`.

Key actors: **Marko Lindqvist** (Redmine user 5, GitHub `cazfi`) is the lead and,
in practice, the only committer. Recurring outside contributors in 2025-26:
Dean Brown (Redmine 21), Michael Ortmann, Alina Lenk, Michal Mazurek (`akfaew`),
Caeies, John Robertson (author of #1791), darkdi, Tobias Frost (Debian).

## 0. The one-paragraph model

Redmine is the system of record. Every change, no matter how small or who wrote
it, gets exactly one Redmine ticket, and the ticket number goes in the commit
message as `See RM #NNNN`. The canonical delivery vehicle is a `git format-patch`
file attached to the ticket; GitHub PRs are tolerated as a *source* of patches but
are never merged through the GitHub UI (0 of the 25 code PRs sampled were; every
one was closed with "Merged outside github UI." after Marko opened a ticket for
it). Marko moves the ticket to **In Review** when he has a patch he intends to
commit, pushes it to every target branch himself (writing the backports when
needed), and moves it to **Closed** ~1-3 weeks later. Contributors never merge,
never close, and are not expected to set In Review.

## 1. Ticket anatomy

### Trackers
Project `freeciv` has four trackers: **Bug** (id 1), **Feature** (id 2), **Task**
(id 3, used for releases/milestones/doc chores), **Spam** (id 4). Marko freely
re-trackers tickets on arrival, so the choice is not critical:

- #2319 Feature -> Bug, with the note "Considering the wrong error message a bug."
- #2104 "Ruins should not have Railroad/Maglev": Bug -> Feature.
- #2187, #2115, #2379 (Dean Brown filed as Task): Task -> Feature, silently.
- #1618 "Add-to-city always adds specialists" stayed Bug although the contributor
  said "Could call this a feature request rather than a bug."

Rule of thumb from the sample: *wrong behaviour or crash* = Bug (goes to all
branches); *refactor, cleanup, optimisation, new behaviour* = Feature (goes to
main, sometimes S3_3). Pure refactors such as "pf_reverse_map_new() parameter
order" (#1060) and "count_diplomats_on_tile() from server to default AI" (#2243)
are Features.

### Categories relevant to us
Category list (ids): AI 17, Server 2, General 1, client-common 18, Rulesets 5,
Documentation 4, Scripting API 20, CI 16, Bootstrap 3, plus per-client ones.

- `ai/` and `server/advisors/` changes -> **AI**: #1077 "Pass civ_map to
  assess_danger_unit()", #724, #730, #2237 "adv_unit_execute_path(): ...",
  #2308 "advisors/ : NULL -> nullptr", #1831 (daidiplomacy), #2081, #2379.
- `common/aicore/path_finding.c` (pathfinding proper) -> **General**: #1060
  "pf_reverse_map_new() parameter order", #2100 "Negative veteran levels at
  path finding", #1918 "access_areas_refresh(): Check only existing tiledefs".
- Autoworker server code (`server/advisors/autoworkers.c`) -> **Server** in
  #1791 (set by the reporter) — but Marko puts advisors/ under AI (#2308).
- Leaving category empty is fine; Marko fills it in (#1943, #2366, #2374, #2432
  all went `None -> <category>` on his first touch).

### Priority
Normal in 68 of 70 tickets read. Reporter-chosen Low/High is normalised back to
Normal (#1942 High->Normal, #2430 Low->Normal, #2366 Low->Normal). Exceptions
are Marko's own: #730 "Emergency Fix" for a CI build break on S3_1; #1684 raised
to High because it gated the 3.3 "key arrangement freeze". Leave it at Normal.

### Target version (`fixed_version`)
Version names are release numbers, and the convention is: **name the release of
the oldest branch the change goes to**; it is implied that it also goes to every
newer branch.

- `3.2.7`, `3.2.8` = S3_2 (and therefore also S3_3 and main). Used for bug
  fixes: #2319, #2238, #2164, #1942, #2272.
- `3.3.0`, `3.3.0-beta1` = S3_3 + main. #2052, #1893, #2379, #2179.
- `3.4.0` = main only. All the `NULL -> nullptr` refactors, #2334, #2187, #2237,
  #2243, #1918.
- Suffixed milestones: `3.4.0-d3f` ("datafile format freeze", for changes that
  alter savegame/ruleset format: #2184, #2314, #2261), `3.3.0-npf` ("network
  protocol freeze"). Marko moved #2031 (new Lua function) from 3.3.0 to
  `3.3.0-d3f` because it changes the scripting API.
- Contributors do **not** set fixed_version; they write it in a note or the
  attachment description instead ("Main only." #2334; "Patch good for S3_2,
  S3_3 and Main." #2164; "Guessing targeting to 3.3, I could make a separate
  patch for 3.2 if desired." #2115). Marko sets the field when he moves the
  ticket to In Review, and links the ticket as `blocks` the release Task
  (e.g. #2401 "Release 3.2.8").
- Marko can and does retarget conservatively: #1903 "this is also high impact
  low-level change with potential to break many (surprising) things. Let's at
  least initially target to main only to gain confidence to the change, and
  only backport to S3_3 when we have seen no issues in for some time."

### Ticket before or after the patch/PR?
- **GitHub PR first, ticket by Marko afterwards** is the actual pattern for PR
  contributors: "Ticket to handle this: https://redmine.freeciv.org/issues/879"
  (PR #66); "Ticket: https://redmine.freeciv.org/issues/941 (Maybe you can open
  the tickets yourself for your future PRs?)" (PR #69); "Thanks. Opened a
  ticket about this (we need one to track all changes in one place, regardless
  if they came as github PRs)" (PR #48). He plainly prefers the contributor to
  open it.
- **Patch contributors open the ticket and attach the patch in the same
  minute** (#2334, #2104, #2164, #2040, #1997) or describe first and attach a
  few days later (#2052: description 2026-06-14, patch same day; #1903).
- Marko's own tickets: description + patch + status In Review within minutes
  (#1077, #1060, #724, #2237). Discussion-only tickets exist too (#1490 Lua
  API proposal, #1669).

### Title phrasing
Titles are terse, technical, and name the function or file. Two shapes:

- Bug: a noun phrase describing the defect, often starting with the function —
  "execute_orders() calling action_id_get_target_kind(order.action) multiple
  times for the same action" (#2272); "Negative veteran levels at path finding"
  (#2100); "reversed condition in unit activity change test" (#2432, outside).
- Feature: imperative or an `X -> Y` arrow — "Pass civ_map to
  assess_danger_unit()" (#1077); "adv_unit_execute_path():
  game_unit_by_number() -> unit_is_alive()" (#2237); "Optimize
  maybe_become_veteran_real()" (#1001).
- Subsystem prefix with colon where it helps: `gtk4:`, `Qt:`, `Meson:`, `CI:`,
  `sandbox:`, `granularity:`, `S3_2 installer_cross:`. A branch prefix (`S3_1
  ...`, `S3_3: ...`) marks a ticket that applies to one branch only (#730,
  #2358).
- Outside contributors' lower-case titles ("improve error message when secfile
  not found" #2319) are accepted unchanged; Marko occasionally rewrites a title
  to be more specific (#2424 "Segfault when quitting client" ->
  "unit_virtual_destroy(anim->movement.mover) segfault when quitting client").
- Do not put the result in the title: "autoworker end_phase() 145x performance
  improvements." (#1791) is the only title in the sample with a number in it,
  and it is the stalled one.

### Body length and content
Short. Marko's bodies are one to three sentences: what is wrong, where, and
(for Features) what to do instead. Example, #1918 in full: "Currently
access_areas_refresh() iterates tiledef checks up to MAX_TILEDEFS. It should
check only actually existing tiledefs. Just use tiledef_iterate() which will do
the right thing in this respect too. While at it, also change pf iteration loop
to be the outer one and tiledef iteration loop the inner one for slight
performance boost (tiledef iteration is quicker one to run over and over
again)."

Outside contributors' accepted tickets are similar: #2432 is three sentences of
symptom + root cause + "The attached works for me."; #2164 quotes the exact
effect line to add and states "S3_2, S3_3 and Main." Longer bodies (#2052,
#2115, #2179) are tolerated but Marko quotes and answers them piecewise.

What is *not* in bodies: test plans, benchmark tables, or "how I verified".
Marko verifies himself (builds with `-Werror`, runs autogames, loads
savegames, runs clang analyzer) and reports results back as review notes.
When a bug cannot be reproduced he asks for a savegame: "Can you attach a
savegame where this can be reproduced from?" (#1943); "Can you attach a
savegame which you are used for that testing?" (#1791).

## 2. Patch delivery

### Attachment vs PR
Both are accepted as *input*; neither is merged as-is through GitHub.

- 14 PRs have ever been merged via the GitHub button in the repository's whole
  history, all translation (.po) or README updates.
- Every code PR in the sample (#43, #45, #53, #66, #68, #69, #77, #78, #86, #89,
  #92, #93, #94) was closed with "Merged outside github UI." (sometimes "Also
  backported to stable branches." / "Also ported to later branches.") after
  Marko created a Redmine ticket for it.
- GitHub Issues are disabled on the repo; the description says "Report bugs and
  submit patches at https://redmine.freeciv.org/projects/freeciv".
- The patch for main is usually taken from the PR verbatim except for the
  commit message: "Patch in the PR should be fine for main branch, except for
  the commit message needing this ticket referenced." (#2214); "Main branch
  patch is implementation from the PR, only commit message changed" (#931).

Conclusion: the ticket with an attached `git format-patch` file is the
first-class path. A PR is fine as a convenience but we should open the ticket
ourselves and attach the patch there too, so Marko does not have to.

### Patch file naming
Everything is `git format-patch` output (one commit per file, with the commit
message inside).

- Marko: the raw `git format-patch` name from his local queue, e.g.
  `0065-AI-Pass-civ_map-to-assess_danger_unit.patch` (#1077),
  `0089-execute_orders-Call-action_id_get_target_kind-just-o.patch` (#2272).
  The leading number is his queue position and means nothing to anyone else.
  The **attachment description field** carries the branch: `main`, `S3_3,
  S3_2`, `S3_3, S3_2` (#2272 has three files, described `main`, `S3_3`,
  `S3_2`).
- Dean Brown (most prolific outsider): `<ticket>.patch`, with branch suffixes
  when they differ: `2052_Main.patch` + `2052.patch`; `1893_Main.patch` +
  `1893_S3_3.patch` (+ `1893_S3_2.patch` earlier); `1903_S3_3&Main.patch`;
  `2319_Main_Part1.patch`, `2319_S3_3_Part1.patch`, `2319_Part2.patch`. The
  description says the rest: `good for S3_3 & Main`, `all branches`, `Also good
  for S3_2`.
- Others: `1791.patch`, `1761.v2.patch` (John Robertson, a version suffix);
  `unitchg.patch`, `techrptfix.patch` (Andrew McGuinness, descriptive);
  `freeciv-3.2.5-sdl2-include.patch` (Matt Jolly);
  `0001-tolua-Generate-deterministic-code.patch` (Bernhard Wiedemann, raw
  format-patch). All accepted; naming is not policed.
- Revisions: delete the old attachment and upload a new one with the **same
  file name** (Dean Brown does this every round: journal shows
  `2187.patch->None` then `None->2187.patch`). Marko instead uploads a second
  file with the same name and a note ("Do not break from the loops..." #1918).

### One ticket = one commit (per branch)
Strictly enforced in both directions.

- Several logical changes in one PR: "The PR has 3 commits, and they really are
  separate things (should not be be squashed) -> we will need 3 separate
  tickets as well, to track each improvement separately (they may go in to
  different branches and/or different times)" (#2186 -> #2221, #2222).
- Fix-up commits on top: "Squash these and force push to this PR, so it's one
  commit to the version history." (PR #68); "please don't just add new commits
  making the fixes on top of the original ones, but fix the main commits and
  force push." (PR #90); "In the future, please provide one change in a one
  patch/commit. Now this PR had three, and we don't want those broken
  intermediate steps in to the git history. Use force-push to the PR as needed.
  (But also; don't include separate changes to the same commit/patch. Each
  change should have their own commit)" (PR #96).
- Two patch files for one change: "Combine as a single patch, these make no
  sense as separate patches (it's just broken when you have just one applied)"
  (#2115).
- A follow-up found during review of a resolved ticket: "#1618 has been
  resolved already. Open a new ticket for the new change." (#2187). Scope creep
  inside a ticket: "Fixing this in general registry code instead of rulesave
  seems like a good idea, but then should be done with a separate ticket scoped
  correctly (and maybe going also to S3_2) -> #2247. This ticket should revert
  to the previous version of the patch it had." (#1893).

### Backports to stable branches
Same ticket; one patch file per branch where the patch differs; one file if it
applies everywhere. Marko writes the backport himself if the contributor does
not, and credits it in the commit body.

- "Patch from the PR applies to main branch only. Older branches need rebasing
  of the patch." then he attached `0035-AI-Limit-diplomacy-target-iterations-
  to-alive-player.patch` described `S3_3, S3_2` (#1831).
- "Original patch is fine for S3_2 and S3_3. Attached patch for main branch
  that also updates gtk4x-client." (#2430).
- "Backported to S3_3 & S3_2" (#2122); "Even S2_6 patch included" (#931).
- He does ask when he wants the contributor to do it: "Can you provide patches
  also for S3_3 and S3_2 branches, as requested in the ticket?" (PR #73).
- Branch policy for non-bugfixes: "The patch should be first made for the main
  branch. We can then think which stable branches, if any, we want to backport
  it to." (PR #65, a performance change).
- Stable-branch gameplay changes are weighed separately: "For S3_2, fixing this
  will make quite a big gameplay change for an already very mature branch. I'm
  a bit undecided..." then "Let's test first, before making the decision." then
  after autogames "This makes no big difference in autogames -> doesn't break
  the gameplay. I consider this ready to go to S3_2 too." (#2164).
- Rebase requests are blunt and reference the conflicting ticket: "Has a
  textual conflict with #1766. Rebase on top of that." (#1791); "Main patch
  conflicts with #2323 - please rebase on top of that." (#2319); "Main patch
  does not apply (conflict at common/traderoutes.c)" (#2179). Ordering is then
  recorded with a `blocks` relation.

### Status workflow (observed, 70 tickets)
```
New ──(Marko: patch acceptable)──> In Review ──(pushed to all branches)──> Closed
 │                                     │
 │                                     └─(review finds a problem)──> In Progress,
 │                                        assigned back to contributor; they
 │                                        re-attach; Marko -> In Review again
 └──(declined)──> Rejected
```
- **In Review is set by Marko**, together with `assigned_to = Marko` and
  `fixed_version`. In 70 tickets no outside contributor set it. The only
  non-Marko person to set it was Alina Lenk (a committer) on her own #554, and
  she reverted it to New when a thread-safety problem surfaced.
- "In Review" means "I intend to commit this; waiting for objections", not
  "please review": "It's now 'In Review', waiting to be merged to the freeciv
  git repository, if there's no review findings." (PR #96).
- **Resolved** is essentially unused (once, #730, by Marko for his own CI fix).
- **Closed** is set only by Marko, when the commit is on every target branch.
  Typical In Review -> Closed latency: 6 days (#2334), 9 (#2238), 14 (#2319),
  22 (#1077), 28 (#2052), 33 (#2237). Dates cluster: he pushes batches.
- **Rejected** carries a one-paragraph reason (#1799 broken refactor; #1900 /
  #1901 design disagreement, resolved by his alternative #1919; #1960 his own
  patch that "breaks the production target selection").
- Duplicates are Closed with a pointer: "Fixed in #2238" (#1943); "Will be fixed
  by #2034." (#2060).

## 3. Commit message style

The authoritative text is the wiki page `Commit_rules#Commit_message`
(https://www.freeciv.org/wiki/Commit_rules, added 2025-11-29 to close #1494
"CodingStyle: Add commit message guidelines"; the page is behind a JS challenge
but readable through the MediaWiki API). Verbatim:

> * Subject line should be in imperative form, and not overlong
> * Subject line may begin with a module name or other location identifier followed by ':'
> * There must an empty line between the subject line and the body of the commit message
> * There must be an empty line after body of the commit message, followed by credits (if any)
> * By default all credit goes to git commit author. If somebody else is responsible of a part of the commit, credit must be given to them. Typical categories are "Reported by ???", "Requested by ???", "Patch by ???", but there can be others too. Further, by default the "patch by" author, even when different from git commit author, gets also all other credit, so other categories are not needed when they have also reported etc the commit.
> * If there has been any credits, another empty line ,must be placed after them
> * Next must be reference to the ticket in the tracker. This is tracker instance, currently always RM (for redmine.freeciv.org), followed by the ticket number.
> * Preferably add also another empty line followed by "Signed-off-by" -line (which 'git commit' fills when run with '-s' switch)
>
> Example commit message:
> ```
> Docs: Improve commit message instructions
>
> Add complete list of requirements for
> an acceptable commit message.
>
> Requested by John "contributor" Doe
>
> See RM #1494
>
> Signed-off-by: Marko Lindqvist <cazfi74@gmail.com>
> ```

The same page also states the general rule: "Patches must be posted to the bug
tracker before commit in almost all cases." and describes the intended review
cadence ("set the status to *In Review*. Allow an adequate period of time in
this state for comments (minimum 36 hours) ... re-review (minimum 24 hours)").
Also: "When you start working on a ticket, ensure that it's assigned to
yourself and set the status to *In Progress* to avoid duplication of effort."
(Outside contributors cannot assign to themselves; Marko does it for them when
bouncing a patch back.)

### What the history actually shows (last 2500 commits on `upstream/main`)
- `See RM #NNNN` in 2095 commits; `See osdn #NNNN` in 111 (the pre-2023 tracker;
  `See hrm #` even older, seen in PR #14, #22). Every non-maintenance commit
  has exactly one such line. There is **no** `Fixes:`/`Closes:` convention
  (3 occurrences total) and no GitHub `#NN` cross-refs.
- `Signed-off-by` in 2378: Marko signs all his commits; committers Alina Lenk,
  Ihnatus, Matt Jolly sign theirs. Patches from outside contributors land
  **without** a Signed-off-by and nobody adds one (Dean Brown's 69 commits,
  John Robertson's 8, Michael Ortmann's 3, akfaew's 2). It is "preferably", not
  a DCO.
- `Reported by` 64, `Requested by` 35, `Patch by` 1. No `Co-authored-by`, no
  `Tested-by`, no `Reviewed-by` anywhere. Co-authorship is expressed in prose
  in the body: "Minor changes by Marko Lindqvist" (Ihnatus, 760a44ee11), "Minor
  modifications by Marko Lindqvist" (Tobias Frost, 7274ec2012), "Ported to S3_2
  by Marko Lindqvist" (9cf1d56eb5), "Backported by Marko Lindqvist"
  (e0420c9d15).
- The credit line matters to Marko because it feeds the PEOPLE page: "It's
  important to include the reporter credits to the commit message, as that's
  where they are collected from to https://www.freeciv.org/wiki/PEOPLE" (#2085,
  where Dean had written "reported by John Campbell on the forum" only in the
  ticket).
- Authorship is preserved from the patch; Marko asked John Robertson to use
  format-patch for exactly this reason: "you may want to do the patch with 'git
  format-patch' instead of just 'git diff' to include meta information like
  your authorship over the patch." (#1646).

### Title shape
Imperative, capitalised, no trailing period, optional `location:` prefix. Among
the last 300 titles the leading tokens are `CI:` 15, `gtk4(x):` 14, `Update`
12, `Remove` 11, `Qt:` 11, `Comment` 11, `Meson:` 9, `gtk:` 9, `Add` 7, `Make`
6, `Fix` 6, `Bump` 5, plus bare function names (`random_movements(): ...`).
Area prefixes seen for our areas: `AI:` ("AI: Limit diplomacy target iterations
to alive players only", "AI: Pass civ_map to assess_danger_unit()"),
`autoworker:` ("autoworker: Enforce API consistency regarding the handling of
unit paths", e08d29381e), `path finding:` ("path finding: Check against
negative EFT_VETERAN_BUILD"), `savegame:`, `client:`, `advisors/` only in
ticket titles.

### Marko rewrites contributor titles into this shape before pushing
He does not ask; he just does it and says so ("I'll modify the commit messages
of this one myself (no need to submit corrected version)", #2085):

| contributor wrote | landed as |
|---|---|
| "Fix iterators" (PR #78) | "AI: Limit diplomacy target iterations to alive players only" + "Reported by Marko Lindqvist" |
| "gtk4: fix proposal for resize stuff." (PR #94) | "gtk4(x): Fix message area resize" |
| "gtk4: synchronize update_animation with idle tick." | "gtk4(x): Synchronize update_animation() with idle tick." |
| "Fix leak in cityrep.c:cityrep_sort_func()" (PR #69) | "gtk: Fix leak in cityrep.c:cityrep_sort_func()" |
| "improve error message when secfile not found" (#2319) | "Improve error message when secfile not found" |
| ticket "improve format of output from ruledit/ruleup" (#1903) | "registry_ini.c: Optionally store strings with newlines added" |
| ticket "[proposed] resolved inconsistency in autoworker APIs..." (#1701) | "autoworker: Enforce API consistency regarding the handling of unit paths" |

Bodies are kept short. A body is optional for self-explanatory titles (most of
Dean Brown's commits are title + `See RM`). When present it is 1-6 lines of
*why*, occasionally a `-` bullet list of sub-changes (e1ad2a2748, dab3a56c51).
Nothing in the sample mentions benchmarks, test commands, or tooling used.

### AI assistance
No commit, ticket, or PR comment in the sample mentions AI/LLM assistance in
either direction (Redmine search for "ChatGPT", "Copilot", "LLM",
"AI-generated": zero relevant hits). The GitHub repo has Copilot PR review
enabled (auto-comments on PR #77, #78) and Marko ignores its output. PR #78's
body reads machine-assisted (file:line bullet list) and was merged after a
title rewrite, without comment on provenance. See §6.

## 4. Review customs

### Who reviews, who merges, who closes
Marko reviews, merges, backports, and closes. Nobody else merged anything in the
sample; no contributor ever set Closed. He reads every patch himself and tests
it: builds (with `-Werror`, clang analyzer, `-Doptimization=2` per #1477), runs
"autogames" (headless AI-vs-AI games with sanity checks on), loads savegames.
Findings come back as terse notes, often with a pasted compiler line or
backtrace:

- "Attempt to build the patch result in: 1) Number of errors that look like
  you have forgotten to include packets.def changes ... 2) Number of 'no
  previous prototype for ...' -errors" (#1791)
- "I got an autogame crash with this. 'realloc(): invalid next size' ..." then,
  40 minutes later, the exact faulty loop quoted and diagnosed (#1903)
- "clang analyzer: ... warning: 2nd function call argument is an uninitialized
  value [core.CallAndMessage]" (#1903)
- "When client connects, server says: 1: Trying to put -1 into 8 bits; it will
  result 255 at receiving side." (#2187)
- "I got an autogame sanity check failure 'in auto_arrange_workers() ...
  citizens not equal [size], repairing ...', and this patch seems likely
  reason." (#1618)

### What he objects to (ranked by frequency in the sample)
1. **Unrelated changes bundled in.** "Step 1: Remove unrelated (and in many
   cases plain wrong) changes from the patch. If there are unrelated, but good,
   changes please open new tickets for them." (#1791). "Is the requirements.c
   change even related? Seems wrong in any case" (#2187). "This ticket should
   revert to the previous version of the patch it had." (#1893).
2. **Ruleset assumptions / hardcoded names.** "That's completely broken
   approach. You are assuming that the ruleset has specific extras, even
   specifically named ones. You should look in the specific properties of the
   extras you are interested about, and to be prepared for the ruleset to have
   any number of extras (incl. zero) that match your criteria. For example,
   look at how your patch would work with the alien ruleset." (#1791). "Do all
   these rulesets have Maglev?" (#2104). "REQ_RANGE_PLAYER check breaks when
   pooled research is enabled" (#2052, twice).
3. **CodingStyle.** Always by pointer to the file: "rereading doc/CodingStyle
   and the patch" (#1791); "Indentation is off (See doc/CodingStyle)" (#2221);
   "Missing empty line between variable declarations and code"; "Do not use
   'default' case but list all the ranges explicitly, so that when support for
   a new range is added, the compiler will error" (#2052); "'pentry' is often
   declared in unnecessarily wide scope. Variables should be declared in the
   innermost block possible." (#1893); "Use 'nullptr' instead of 'NULL' in
   comments too." (#2179); "Please rename so that it's not using the same name
   as an existing (and relevant) function" (#2052); stale function header
   comments (#2221); "Double semicolon in the end." (#2379).
4. **Layering and dependencies.** "you are including fc_types.h from common/ to
   under utility/. That's not allowed. The whole point of separation between
   utility/ and common/ is that utility is independent from common." (#1903).
   "That's a derived dependency, not something we control ourselves, and server
   ... should be buildable without glib" (PR #65). "I don't see you handling
   autotools based build either." (#2115). "please check if other gtk-clients
   need the same change" (PR #68).
5. **Behaviour change not acknowledged.** He accepts them but wants them
   explicit: "Just a note (maybe for NEWS), I'm not against the patch: This
   introduces a rule change that buildings now provide full benefit still at
   the turn they are sold" (#2087). "Does this patch (as it's currently
   implement) change anything?" (#2334). "Can you be a bit more verbose about
   the kind of improvement(s) this patch brings." (#1669).
6. **Silent error handling.** "Instead of silently (not letting anyone to know
   about it) ignoring such error situation, and doing not-necessarily-sane
   arrangements to 'fix' the situation, I would handle this by adding
   fc_assert_ret()" (#1695). Also #2031: "it makes no sense to return
   specifically -1 on error" when -1 is a valid value.
7. **API shape.** "how it selects the low-level behavior based on what the
   high-level entry name happens to be. That's not a good API." (#1903); "That
   seems like a lot of duplication. I would add an helper function
   unit_current_good_replace_priority()" (#2179); "Do those functions need to
   figure out unit_current_goods() for other reasons too, or could that part
   too be incorporated/hidden ... If not, maybe rename the function." (#2179).
8. **Overriding the user.** "I don't think we should override user's control in
   this." / "We should respect user's choice in this." (#1900, rejected).

Never asked for in the sample: unit tests, benchmark numbers, design documents,
screenshots (for non-GUI work), or a changelog entry.

### How requests are phrased
Declarative and short, no softeners, often a question that is really an
instruction: "Isn't the auto_arrange_workers() in the end converting the
specialists to field workers even if they are, for less than a millisecond,
specialists of other type than DEFAULT_SPECIALIST?" (#2334). He quotes with
Redmine's "X wrote in #note-N:" and answers point by point. Acknowledgement of a
fix is minimal: "Yes, that's an improvement." (#1903); "Fixed" (own); status
flip to In Review with no text (most common). He occasionally explains his
reasoning at length when declining (#1900, #1903 API paragraph, #2164).

### Turnaround
- First reaction to a new patch: usually same day (#1791 25 min; #2052 5 h;
  #2187 within a day of the re-upload; #2432 within hours). Big or awkward
  patches can sit: #1903 waited 4 months (Jan -> May) for the substantive
  review, then another 2.5 months; #2052 waited 6 weeks for round two.
- He nudges stalled contributors after weeks or months: "Any progress with
  this? Or can we stop blocking #1893 with this" (#1903, 2.5 months); "Ping"
  (#2186, 5 weeks); "Any plans to fix this patch?" (#1791, 10 months).
- Review rounds are many for larger patches: #1903 went through 11 uploads,
  #2052 five, #1893 five, #2187 three. Each round is one or two findings.

### Contributor-side habits that work
- Re-upload with the same filename and a one-line note: "Fixed." / "fixed" /
  "This is better." / "Take 2, forgot to account for blocked by needing
  improvement."
- State branch applicability in the note or description every time.
- Concede quickly and move on: "Yes, requirements.c should not change." (#2187);
  "Yeah that Main patch was bad - not understanding the new superspecialist
  feature." (#1618); "I must have misunderstood how it was originally working."
  (#1791).
- Offer to split: "Would be a separate issue" (#2052); "Needs a separate issue."
  (#2087); "Created #2187" (#1618).

## 5. Performance work specifically

### How accepted speedups were evidenced
Mostly **not with numbers**. Accepted performance changes are justified by a
one-sentence algorithmic argument, and Marko's acceptance is about whether the
code is obviously right, not about a measurement:

- #1001 "Optimize maybe_become_veteran_real()": "by avoiding need to look up
  unit's veteran system twice." No numbers. Backported to S3_2/S3_1.
- #1918: "change pf iteration loop to be the outer one and tiledef iteration
  loop the inner one for slight performance boost (tiledef iteration is quicker
  one to run over and over again)." No numbers.
- #2272: motivated by a clang analyzer warning, not speed, though it removes
  repeated calls.
- #2432 (outside, Andrew McGuinness): "it's probably at least a small
  optimisation" — Marko worked out the actual behavioural consequence himself
  and accepted it as a Bug for 3.2.8.
- #879 / PR #66 (Michael Ortmann): the only accepted change with a measurement,
  and it is one line: "each `tilespec_reread_frozen_refresh()` on AMD Ryzen
  5700G with `-march=native -O2` takes user 177ms sys 12ms". Accepted in 2
  days, no comment on the number.
- PR #65 (same author, "Speedup freeciv-server cold start by 19%"): a
  paragraph of profiling narrative ("millions of utf8 to utf16 conversions",
  "Schlemiel the Painter's Algorithm"). Marko: "your work sounds interesting.
  And even if it turns out that we can't use the code, it's good to know that
  this is the place to optimize." Blocked on a glib dependency and on "The
  patch should be first made for the main branch." Still open after 2 years —
  the contributor never reworked it.
- #1817 / #2186 (100% CPU): Marko ran his own counter before accepting: "I did
  a test run *without* this patch counting times selection_callback() was
  triggered from real_players_dialog_update(), and it seemed to happen only a
  couple of times on turn change - so this doesn't seem like a major CPU hog to
  me, especially not a constant one. Of course, the patch still seems like a
  nice improvement, so we should get it in."

So: numbers are welcome but not required; what is required is that the change
is small, obviously correct in all rulesets, and separable. A performance claim
that comes bundled with behaviour changes is treated as a behaviour change.

### Debug / verification switches
No ticket in the sample added or removed an environment-variable or log-level
switch for verification, so there is no direct precedent either way. Indirect
signals:
- Dev-only compile switches exist and are removed at branch time by Marko
  himself (#2234 "Drop FREECIV_DEV_SAVE_COMPAT_3_3").
- Sanity checking is a build-time option he uses in autogames ("When sanity
  checking is enabled, there's this 'repairing' part, but in a release version
  new citizen would not be made anything by that either." #1618). A
  consistency oracle that lives behind the existing sanity-check machinery fits
  the house pattern; a new env var in production paths has no precedent.
- Log-level choices get questioned: "Also questionable why the log level gets
  changed from LOG_NORMAL to LOG_VERBOSE" -> "I guess to reduce spam from a
  common (and acceptable?) situation." (#2379). HACKING says log_debug/
  log_verbose text is not translated.
- His own preferred failure mode is `fc_assert_ret()`, not silent fallback
  (#1695).

### The fate of #1791 (autoworker 145x)
Filed 2025-11-30 by John Robertson as Feature/Server with a 114 KB patch,
title "autoworker end_phase() 145x performance improvements.", body: "Autoworker
performance improvements from 1 hr, 24 min, 35 sec to 35 seconds in the
end_phase for the given saved game (a 99.3% or 145x improvement). Please ask
design questions and make recommendations. This approach is more aggressive and
less manually directed."

Timeline:
- +25 min, Marko: "Step 1: Remove unrelated (and in many cases plain wrong)
  changes from the patch. If there are unrelated, but good, changes please open
  new tickets for them. There's also some (not many) style issues..."
- +6 h: does not build (packets.def missing, prototypes); "Has a textual
  conflict with #1766. Rebase on top of that." Sets `blocked by #1766`.
- Day 3: v2 patch (84 KB). Contributor asks for design help on client/server
  worker-task communication. No answer to that.
- Day 5, Marko: "Can you attach a savegame which you are used for that testing?
  Sounds very curious if *autoworker* part takes over an hour." and, 20 min
  later, the hardcoded-extras objection ("look at how your patch would work
  with the alien ruleset").
- Day 11: savegame attached; contributor explains the real change: "Removing
  the recursion, and choosing the closest task is the crux of the proposed
  speed improvement. Additionally, I took the liberty of changing the
  autoworkers to be more autonomous according to my own personal sentiments."
- Day 12-13: Marko corrects a misunderstanding of how autoworkers work;
  contributor: "I must have misunderstood how it was originally working."
- 10 months of silence. 2026-10-01, Marko: "Any plans to fix this patch?
  (Though I'm afraid that the ways the patch is broken is what also brings the
  performance improvements, and making it to work properly would lose those
  gains)". Status still New, unassigned, no fixed_version.

The same contributor got eight other patches in during the same period
(#1646, #1701 "autoworker: Enforce API consistency regarding the handling of
unit paths", #1947, #1807, #1808, #1827, #1832, #1852), each small, each one
thing; for #1701 Marko even said "Might take a while still before this gets
merged, as I want to wait for the next autogame testing run and test this there
first." So the lesson is not "autoworker performance is unwelcome"; it is that
the 145x patch (a) mixed a speedup with a design change, (b) hardcoded ruleset
names, (c) did not build, (d) gave a wall-clock number from an extreme savegame
("over 10K autoworker units") that made the maintainer doubt the measurement
rather than trust it, and (e) asked the maintainer to do the design.

## 6. Policy signals

### AI-generated / AI-assisted contributions
No stated policy anywhere reachable: not in the repo (`doc/HACKING`,
`doc/CodingStyle`, no `CONTRIBUTING`, `.github/` holds only `workflows/`), not
in the Commit_rules wiki page, not in any ticket or PR comment. Redmine
full-text search for "ChatGPT", "Copilot", "LLM", "AI-generated" returns
nothing relevant. Observed behaviour:
- The GitHub repo has Copilot code review switched on; it comments on every
  PR (PR #77, #78). Marko never replies to it.
- PR #78 (akfaew, "Fix iterators", Dec 2025) has a body in the bullet-per-
  file:line style typical of LLM output. Marko merged it in three weeks with a
  rewritten title and "Reported by Marko Lindqvist", with no remark.
- A contributor quoted "Google AI says" in a ticket discussion (#1817) with no
  reaction.
- Marko's objections are always to the content (unrelated changes, ruleset
  assumptions, style), never to provenance.
Conclusion: there is no requirement to disclose and no precedent for
disclosure; equally nothing forbids a one-line mention. The project's actual
filter is "one small, correct, style-conformant change per ticket", applied
regardless of how the patch was produced.

### Copyright / CLA / sign-off
- No CLA, no copyright assignment, no DCO. Freeciv is GPLv2+ and nothing asks
  the contributor to state that; one contributor volunteered "contributed
  under the project's GPL-2.0 license" (PR #96) and it was not remarked on.
- `Signed-off-by` is "preferably" (Commit_rules) and is in practice added only
  by committers; outside patches land without it.
- Authorship is preserved via `git format-patch` metadata; Marko asks for
  format-patch over plain diff for that reason (#1646). Credits for others go
  in prose lines (`Reported by`, `Requested by`) because they are harvested for
  the PEOPLE wiki page (#2085).

### Code style gatekeeping
`doc/CodingStyle` is the reference and is cited by name (#1791, #2221). It is
itself maintained through tickets (#1477 test builds with
`-Doptimization=2`; #1347 meson options; #648, #1147, #2341 listing `fc_*`
replacements for libc functions). Things enforced in the sample that are in
CodingStyle: 2-space indent, blank line after declarations, innermost-scope
declarations, `nullptr` not `NULL` (also in comments), explicit `switch`
cases without `default`, `fc_strerror(fc_get_errno())` and other `fc_`
wrappers over raw libc, function header comment blocks kept in sync with the
code, utility/ must not include common/.

### Branch policy
Bug fixes go to every maintained branch (currently S3_2, S3_3, main). Features
and refactors go to main; S3_3 while it is in beta if low-risk. "high impact
low-level change ... Let's at least initially target to main only to gain
confidence to the change, and only backport to S3_3 when we have seen no issues
in for some time. (Some would argue that we should do that for all patches - to
first test in development branch for a while before promoting to stable
branches)" (#1903). Network-protocol- or format-changing work must land before
the corresponding `-npf` / `-d3f` milestone of the next branch.

## 7. Social norms

### Tone
Marko writes in a dry, technical, zero-ceremony register: no greeting, no
sign-off, rarely "thanks" ("Thanks. Opened a ticket about this" PR #48 is the
warmest line in the sample). Disagreement is direct — "That's completely broken
approach" (#1791), "The new assignment does nothing sensible" (#1799), "That's
not allowed" (#1903) — but always followed by the reason and usually by what to
do instead. He also admits his own errors plainly: "Oh. I thought it was new
with your recent changes." (#1618); "Correction: It was not a server crash, but
the client lost the connection on its own." (#2366); "Patches based on the
assumption that it is." (#2374).

Contributors range from the same register (Andrew McGuinness, Michael Ortmann,
Alina Lenk) to chatty (Dean Brown: "Don't you just hate it when...", "Setting a
personal record for most failed attempts at a patch, but I'm motivated to keep
trying til I get it right."; Caeies: "Hi all, ... Best regards."). Both are
fine. Nobody is thanked for a merged patch; the Closed status is the thanks.

### How to disagree
With a technical argument, once, and then either concede or split. Dean Brown
in #1893: "Why can't we just not output empty strings at all? I looked into
that and it seems possible, but there's a catch indicated by a comment in
rulesave.c ... So would take more work, and be out of scope for this issue. I'm
not going to mess with it - it's not broke, don't fix it." — accepted. John
Robertson in #1900/#1901 argued design for two rounds; Marko answered the
substance ("There are many good reasons why user has sentried the worker..."),
then opened his own alternative (#1919 "Autoworkers: Seek shelter when no
work"), shipped it, and only then Rejected the originals with "That's done now.
Closing this one."

### Ask before coding?
Not required, and asking without a patch gets a slow or no answer (Caeies in
#2186: "How deep should I investigate / take time to try fixing the gtk4
client?" — unanswered; John Robertson in #1791: "Please ask design questions
and make recommendations" — answered only with defects). What does get
answered quickly is a *concrete* design question attached to a concrete
problem (#554 thread-safety: Marko replied within a day with "Adding
thread-local storage functionality to fcthreads should not be too hard, if we
avoid over-engineering it"; #1490 Lua API proposal: a pointed question about
server-side CMA cost within two days). Discussion-only Feature tickets exist
and are the de-facto RFC mechanism (#1490, #1669, #1900), but they do not
move until a patch appears. There is no RFC tracker, mailing-list design
process, or roadmap document in evidence; the forum
(forum.freeciv.org) is where users report things that contributors then
ticket ("reported by John Campbell on the forum", #2085/#2087).

### How to split large changes
- One ticket per independently-committable change; use Redmine relations:
  `blocks` for ordering ("This has textual dependency to #2221", #2222) and
  `relates` for context. Marko adds these himself when he sees a dependency.
- Say where the piece came from in the first line: "Spun off from #1618."
  (#2187), "Split from #2186" (#2221), "Follow on to #1618 and #2187" (#2334),
  "Inspection of goto_tile setters inspired by #1695 reported by John
  Robertson" (#1766).
- Order: bugfix first (all branches), improvement second: "personally I would
  arrange them the other way around - to do bugfix going to all branches first,
  and improvement going to yet-undecided branches second, so the bugfix patch
  would not need to be so different between different branches." (#1893).
- When a review spawns a new change, Marko says "Open a new ticket" rather
  than letting the current one grow (#2187, #1893 -> #2247, #2366 -> #2368,
  #2369).
- Series of mechanical changes are one ticket per file or directory, filed in
  bulk (the `NULL -> nullptr` series: #2308 `advisors/`, #2232 `advgoto.c`,
  #2282 `infracache.c`, #2354 `daisettler.c`...). Dozens of tiny tickets are
  normal; one big ticket is not.

## 8. Checklist for our submissions

### Ticket (Redmine, project `freeciv`)
```
Tracker:        Bug  (wrong result / crash / assertion; goes to S3_2+)
                Feature (speedup, refactor, new behaviour; goes to main)
Category:       AI      for ai/*, server/advisors/*
                General for common/aicore/path_finding.c, common/*
                Server  for server/* outside advisors
Priority:       Normal  (leave it)
Target version: leave empty; state branches in the first note instead
Assignee:       leave empty
Subject:        <function or file>: <imperative or defect noun phrase>
                e.g. "pf_reverse_map: Do not recompute lattice per callback"
                     "assess_danger(): O(units^2) scan of guard radius"
Description (3-8 lines):
  <what is wrong / what is slow, where (function names), why it matters>
  <what the patch does instead, one or two sentences>
  <ruleset-independence statement if any extra/unit/terrain is touched>
  <"Patch applies to main; S3_3/S3_2 variants attached" or "main only">
  <Reported by / origin if the problem came from elsewhere>
Attach:         git format-patch output, one commit, named
                  <ticket>.patch            (applies to all target branches)
                  <ticket>_Main.patch, <ticket>_S3_3.patch ... when they differ
                attachment description = branch list ("main", "S3_3, S3_2")
                plus a savegame that reproduces/exhibits the issue when relevant
Do not:         set In Review; set fixed_version; put numbers in the title
```
Revision: delete the old attachment, upload the new one under the same name,
one-line note ("Fixed." / "Rebased on #NNNN." / "v2: dropped the unrelated
hunk in X").

### Patch / PR mechanics
- Patch on the ticket is primary. If we also open a GitHub PR, open the ticket
  first and reference the PR in the ticket and the ticket in the PR body; PRs
  without a ticket get one opened *for* us with a mild rebuke (PR #69).
- One PR = one commit = one ticket. Review fixes are squashed into that commit
  and force-pushed; a PR with N independent commits will be asked to become N
  PRs/tickets (#2186).
- Base the patch on `main`. Attach S3_3/S3_2 variants ourselves if the change is
  a Bug; if it is a Feature, say "main only" and let Marko decide backports.
- Rebase when told "conflicts with #NNNN" — he will not resolve conflicts for
  big patches (he will for one-liners, and says so).
- Expect: first reply within a day; In Review when he is satisfied; Closed 1-4
  weeks later after it reaches all branches. Expect him to retitle the commit.

### Commit message template
```
<area>: <Imperative sentence, no trailing period>

<Optional 1-6 lines on why; what was wrong; what it does now.
 Note explicitly if observable behaviour changes.>

Reported by <Name>          (only if someone else found it)

See RM #NNNN

Signed-off-by: <Name> <email>   (optional; harmless)
```
Areas seen upstream: `AI:`, `autoworker:`, `path finding:`, `advisors:` (ticket
only), `server:`, `common:`, or just `<function>():`. No `Fixes:`, no
`Co-authored-by:` (write "Based on a patch by X" / "Minor changes by X" in prose
if needed), no benchmark tables, no tool mentions. Our own repo rule 5
(`<area>: <imperative verb> <object>`) is already compatible; drop our
`Co-Authored-By`/`Claude-Session` trailers before upstreaming — upstream has
zero `Co-authored-by` lines in 2500 commits and the Commit_rules page defines
the trailer block as credits + `See RM` + optional `Signed-off-by` only.

### The four things most likely to get the pathfinder PR bounced
1. **Bundling.** A speedup that also changes which path is chosen, or touches
   `ai/default` behaviour, or carries drive-by cleanups, gets "Step 1: Remove
   unrelated ... changes". Ship the pure-identity speedup (same results, faster)
   as its own ticket; file behaviour changes separately with their own
   justification, and mark them `blocks`/`relates`.
2. **Ruleset assumptions.** Any `*_by_rule_name("...")`, any assumption about
   the number or kind of extras/terrains/unit classes, any constant that is
   really a ruleset property. Test with `alien` and `granularity` as well as
   `civ2civ3`/`classic` and say so. This is the single objection he restates
   most often, and it is what killed #1791.
3. **A verification switch or debug knob in production code.** No precedent
   for env-var oracles; his instinct is `fc_assert*` and build-time sanity
   checks. Either keep the oracle out of the upstream patch entirely (keep it in
   our tree), or make it compile-time under the existing sanity-check /
   `FREECIV_DEBUG` machinery and say in the ticket why.
4. **Build or style misses he finds in the first ten minutes.** He applies the
   patch, builds with `-Werror -Doptimization=2`, runs clang analyzer and an
   autogame before reading closely. "no previous prototype", "set but unused on
   ndebug build", wrong indentation, `NULL` in a comment, a stale function
   header, a new static that is unused on some `#ifdef` path — any of these
   costs a round trip and goodwill. Also: the patch must apply to current
   `main` on the day he looks at it.

A fifth, softer one: **numbers he cannot reproduce**. A measurement from an
extreme savegame invites "Sounds very curious" rather than belief. Give the
algorithmic reason first (as #1001/#1918 do), then one line of measurement on an
ordinary autogame, and attach the savegame.
