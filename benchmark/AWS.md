# Running the campaign on a cloud box

Target: one big compute-optimized instance, on-demand (nobody is
around to resurrect a reclaimed spot instance), agent-optional: the
campaign runs itself; an agent is needed at hour 0 and for the
end-of-run analysis only.

## Instance

- `c7a.24xlarge` (96 vCPU / 192 GB), on-demand: ~$118/day. Enough for
  the full campaign.sh in ~48 h. `c7a.48xlarge` if the primary-regime
  matrix should run at double seed counts.
- 500 GB gp3. Campaign runs export DEEP_QUIET=1, so logs stay sane.
- Jobs: campaign.sh defaults to 0.7 x nproc (SMT threads do not give
  full game throughput).

## Getting the tree there (repo has no remote by design)

    git bundle create deep-ai.bundle --all
    scp deep-ai.bundle box:
    ssh box git clone deep-ai.bundle freeciv && cd freeciv \
      && git checkout <branch>

## Setup

    sudo dnf install -y meson ninja-build gcc python3 zstd git \
      readline-devel zlib-devel libcurl-devel gettext libicu-devel
    # Ubuntu: meson ninja-build gcc g++ python3 zstd git pkg-config
    #   libreadline-dev zlib1g-dev libcurl4-gnutls-dev gettext
    #   libicu-dev libsqlite3-dev  (verified on 26.04, 2026-07-25)
    meson setup build -Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false \
      -Daudio=none -Daimodules=classic,tex,baseline,deep
    ninja -C build

## Launch (unattended)

    sudo tee /etc/systemd/system/deep-campaign.service <<EOF
    [Unit]
    Description=freeciv deep-AI tuning campaign
    [Service]
    Type=simple
    User=$USER
    WorkingDirectory=%h/freeciv
    ExecStart=%h/freeciv/benchmark/campaign.sh
    Restart=on-failure
    RestartSec=60
    [Install]
    WantedBy=multi-user.target
    EOF
    sudo systemctl daemon-reload && sudo systemctl start deep-campaign

campaign.sh is idempotent (every stage is result-cached), so
Restart=on-failure loses nothing. Watch with
`journalctl -u deep-campaign -f` or `tail -f benchmark/results/campaign/campaign.log`.

## End of window

STOP (do not terminate) the instance so the EBS volume keeps caches
and results for analysis, or pull them:

    tar czf campaign-results.tgz benchmark/results/campaign \
      $(ls -d benchmark/results/*-*/metrics.csv 2>/dev/null)

The interesting artefacts: `benchmark/results/campaign/report.md`,
`tune-*/best-tuning.ini`, the holdout logs, and metrics.csv per keyed
result dir.

## New-box bring-up checklist addition (2026-07-29)

A server built without `-Daimodules=classic,tex,baseline,deep` accepts
`create P1 deep` with only a chat-line error ("There is no AI type
deep") and silently substitutes the default AI -- benchmark games
complete normally and mixed-arm objectives cancel to EXACTLY 0.0
(the mirror makes both "sides" identical). This burned ~2.5h on the
first us-east-1 box. Before trusting any new box: run one throwaway
game and grep its log for "no AI type" (must be 0) and "deep-def"
(must be >0). Server-only meson config that works on stock Ubuntu:
  meson setup build -Dclients=stub -Dfcmp=cli -Daudio=none \
    -Dtools=[] -Daimodules=classic,tex,baseline,deep

## Box inventory (2026-07-30)

- us-east-2: i-05162168cf393cb20 (c7a.8xlarge, the original campaign
  box; full toolchain + all aimodules). STOPPED when idle.
- us-east-1: i-0e9a257ef3b781349 (c7a.8xlarge, added 2026-07-29 for
  the parallel regime tune; server-only meson config per the
  bring-up checklist above; key "sweettea", sg deep-tune-ssh).
  STOPPED when idle. Public DNS changes on every start.
Pattern for unattended work: oneshot systemd unit whose ExecStart
ends with `sudo shutdown -h +5 ...; sleep 420; sudo poweroff` (the
double-tap; a plain scheduled shutdown silently failed once).
