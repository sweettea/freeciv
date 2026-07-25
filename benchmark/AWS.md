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
      readline-devel zlib-devel libcurl-devel gettext   # or apt equiv
    meson setup build -Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false \
      -Daimodules=classic,tex,baseline,deep
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
