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

## Box inventory (verified 2026-08-01 against describe-instances)

Two long-lived boxes, both `c7a.8xlarge` (32 vCPU / 64 GB), both
ssh'd as `ubuntu`, both STOPPED when idle (stopped, never terminated:
the EBS volume is the result cache). **Public DNS changes on every
start** — always re-read it, never reuse the last one.

| | **EAST2 — campaign box** | **EAST1 — tune box** |
|---|---|---|
| instance | `i-05162168cf393cb20` | `i-0e9a257ef3b781349` |
| region / AZ | us-east-2 / us-east-2c | us-east-1 / us-east-1b |
| name tag | *(none)* | `deep-tune-east1` |
| key / sg | `sweettea` / `launch-wizard-5` | `sweettea` / `deep-tune-ssh` |
| root vol | `vol-0b814a4e4f8e737f9`, **500 GB** gp3 | `vol-0536838977de54c62`, **40 GB** gp3 |
| AMI | `ami-0e5497a77ef21b5ac` | `ami-052355af2a014bd2c` |
| build | full toolchain, all aimodules | server-only meson (checklist above) |

- **EAST2 is the box of record.** It was the original campaign box and
  its 500 GB volume holds the campaign result cache plus the
  `defence-*` tuning lineage (G, H, Z2, Z3). Anything that needs the
  cache, or a client/tools build, goes here.
- **EAST1 is the parallel-tune box**, added 2026-07-29 so a second
  search could run without contending for the cache. It carries the
  v3 lineage: `~/freeciv/benchmark/results/tuning/v3b` and `v3c`,
  `~/v3b-best.ini`, `~/accept4.log`. Its root volume is only **40 GB**
  — it cannot hold a full campaign cache, so prune result dirs after
  harvesting, or it will fill mid-run.
- A third us-east-2 instance (`i-0e4e0ddb933c30bdc`, t3.medium,
  "Claudeycodey", key `Twell`) is **unrelated** — leave it alone. The
  `--filters Name=key-name,Values=sweettea` below excludes it.

Cost is roughly a third of the 24xlarge figure above, i.e. ~$40/day
each while running — which is why they live stopped.

    # what exists, and is it up?
    for r in us-east-1 us-east-2; do aws ec2 describe-instances --region $r \
      --filters Name=key-name,Values=sweettea \
      --query 'Reservations[].Instances[].[InstanceId,InstanceType,State.Name,PublicDnsName]' \
      --output text; done

    # start, then re-read the DNS (it changed)
    aws ec2 start-instances --region us-east-2 --instance-ids i-05162168cf393cb20
    host=$(aws ec2 describe-instances --region us-east-2 \
      --instance-ids i-05162168cf393cb20 \
      --query 'Reservations[].Instances[].PublicDnsName' --output text)
    until ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=8 \
      ubuntu@$host 'echo up'; do sleep 20; done

    aws ec2 stop-instances --region us-east-2 --instance-ids i-05162168cf393cb20

Pattern for unattended work: oneshot systemd unit whose ExecStart
ends with `sudo shutdown -h +5 ...; sleep 420; sudo poweroff` (the
double-tap; a plain scheduled shutdown silently failed once).
