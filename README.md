### Useful commands

| code                                                                   |                 |
| ---------------------------------------------------------------------- | --------------- |
| `make run_servers`                                                     | run all servers |
| `lsof -i`                                                              | list open files |
| `kill PID`                                                             |                 |
| `nc localhost 9034 < testcases/testcase2-2/3-multiseat.in > 2-2.3.out` |                 |

### TODO

- [x] add 5 second timeout
- [ ] add exit command

### public testcases judge

from [SP2024_HW1_release](https://github.com/NTU-SP/SP2024_HW1_release)

```
python3 checker.py
```

###### Argument

- `-t TASK [TASK ...]`, `--task TASK [TASK ...]`, Specify which tasks you want to run. If you didn't set this argument, `checker.py` will run all tasks by default.
  - Valid TASK are ["1-1", "1-2", "1-3", "1-4", "2-1", "2-2", "3", "4"].
  - for example `python3 checker.py --task 1-1 1-2` will run both `testcase1_1` and `testcase1_2`
