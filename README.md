# DHH UDP

## Compile
```
gcc ./udp_receiver/udp_receiver.c -o receiver
gcc ./udp_sender/udp_sender.c -o udp_sender
```
## Run
```bash
./receiver OUTPUT_FILE.raw 2>&1 | tee 1000Hz.log
```
## Test
```bash
tmux #or so on
./receiver &
./udp_sender
```

