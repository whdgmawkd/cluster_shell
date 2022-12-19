# **Cluster Shell**

**20181619 박종흠**

# Clsh 설계

## 선택 옵션 및 기술적 동작

* 기본구현
    * EventLoop 형태의 Queue를 가지고 각종 처리를 이벤트 단위로 분리하여 핸들링
    * `epoll`: 여러 node의 stdout, stderr을 들어오는대로 처리하기 위해 사용
    * `pipe`: 각 node을 입력 및 출력을 키보드와 화면 대신 부모 프로세스에서 제어하기 위해 사용
    * `thread`: 이벤트 루프를 실행하면서 epoll처리 등의 기능을 동작시키기 위해 별도의 쓰레드 생성
    * `signal`: `SIGPIPE` 시그널은 `epoll`의 `EPOLLERR`을 통해 처리할 수 있어 `SIG_IGN`으로 핸들러 설정

* 옵션1 - hostfile 옵션 생략시 동작 방법 정의
    * `getopt`계열 함수, `getenv`계열 함수를 사용하여 우선순위에 따라 다양한 방법으로 host 설정

<!-- * 옵션3 - 출력 옵션 구현
    * 이벤트 루프 구현에서 OUTPUT 발생 이벤트 처리 부분에 `cout` 대신 `fstream`계열 함수를 통해 파일에 쓰도록 설정
    * `stdout`, `stderr`을 지정한 경로에 `노드명.out`, `노드명.err`파일로 내보내기 -->

* 옵션4 - Interactive 모드 구현
    * 키보드로 부터 입력을 받아 이벤트를 등록하는 별도의 Thread 생성
        별도 Thread로 생성하지 않으면 `cin` 등의 함수에서 block되는 문제가 발생함
    * 이벤트 루프 구현에서 INPUT 발생 이벤트 처리 부분을 추가하여 호스트 명령, 원격 명령, 특정 노드 명령 등 다양한 기능 지원

* 옵션6 - 연결 끊어짐 감지
    * `epoll`에서 PIPE의 쓰기쪽이 닫힌 경우 `EPOLLHUP`이벤트를 통지하므로 이를 감지하여 ssh 연결이 닫힘을 알 수 있음

# Clsh 구현

## 기본 구현

* 개별 원격 노드의 정보를 가지는 `ssh_node`클래스를 생성
* 여러개의 원격 노드를 관리하는 `event_loop`클래스 생성
    * 이벤트를 처리하기 위한 `std::queue`기반 이벤트 큐 생성
        * `std::mutex`, `std::condition_variable`을 사용하여 멀티스레드 환경에서 안정성 확보 및 큐가 비어있을 때 스핀락 등의 CPU 자원 낭비 방지
    * 키보드 입력을 관리하는 `input_handler`함수 생성
        * `getline`을 통해 사용자로부터 입력을 받아 이벤트 큐에 입력 이벤트를 생성
        * 입력의 첫번째 문자는 명령을 실행하기 위한 대상을 정하는데 사용하도록 규칙을 생성함
    * 개별 원격 노드의 출력을 관리하는 `output_handler`함수 생성
        * `epoll`을 사용하여 `stdout`, `stderr`에 발생하는 `EPOLLIN`, `EPOLLERR`, `EPOLLHUP` 이벤트를 처리
        * PIPE가 닫히는 것을 확인하여 특정 Node가 종료되었음을 감지할 수 있음
    * 모든 이벤트를 처리하는 `loop_handler` 함수 생성
        * 위 두 함수에서 발생하는 이벤트를 순차적으로 처리하고, 결과를 화면 또는 파일에 기록하는 역할을 함

## Password 자동입력 구현

* `ssh` 명령어가 암호 입력을 tty장치로부터 직접 받기 때문에 stdin 파이프를 통해 입력이 불가능
* `expect` 패키지에서 이런 특수 입출력을 자동화 해주기 때문에 `ssh_node` 클래스에서 `exec`할 때 `ssh`대신 `expect`명령을 실행하여 암호 자동입력 구현
* 따라서 모든 노드들은 동일 계정/동일 암호를 사용하는 것으로 간주하며, 계정 이름 및 암호는 소스코드를 통해 수정할 수 있음
* **암호가 올바르지 않은 경우 프로그램이 정상동작하지 않음**

**기본 계정 및 암호: ubuntu/ubuntu**

## 옵션1 구현

* main함수에서 `getopt` 및 `getenv` 함수를 사용하여 우선순위에 따라 host정보를 어디서 읽어올 것인가 판단하도록 함
* 아무것도 제공되지 않은 경우 오류메시지를 보여주고 프로그램을 종료
* host정보 우선순위
    1. -h 옵션
    1. --hostfile 옵션
    1. CLSH_HOSTS 환경변수
    1. CLSH_HOSTFILE 환경변수
    1. .hostfile 파일

<!-- ## 옵션3 구현

* 이벤트 루프에서 OUTPUT 이벤트 발생시 플래그 값을 검사하여 파일에 기록하도록 설정 -->

## 옵션4 구현

### 명령어 형식
| 문자    | 동작                                                                     | 예시                |
|---------|--------------------------------------------------------------------------|---------------------|
| ~~`!`~~ | ~~Host에서 실행~~                                                        | ~~`!echo Host`~~    |
| `@`     | 지정한 Remote에서 실행                                                   | `@node1 echo Node1` |
| `%`     | 모든 Remote에서 실행                                                     | `echo Remote`       |
| `^`     | ~~Host와 모든 Remote에서 실행~~ `^exit`의 경우만 프로그램 및 Remote 종료 | `^exit`             |

## 옵션6 구현

* epoll에서 `EPOLLHUP`이벤트가 감지된 경우 해당 PIPE의 write쪽이 close되었다는 의미
* 이는 자식 프로세스가 종료되었다는 뜻이므로 해당 노드의 연결이 정상종료 혹은 다른 이유로 종료 되었음을 의미함
* 해당 노드를 제외한 나머지 노드는 여전히 연결되어 있는 상태이므로 event_loop에서 해당 노드를 제거

# Clsh 결과

## 설치하기

```bash
git clone https://github.com/whdgmawkd/cluster_shell.git
cd cluster_shell
mkdir build
cd build
cmake ..
make
```

## 빠른 시작

* 기본 사용법
```bash
./clsh -h node1,node2,node3 cat /proc/loadavg
```

* Interactive Mode 사용법
```bash
./clsh -h node1,node2,node3 -i
```

* 파일로 출력 사용법
```bash
./clsh -h node1,node2,node3 --out /tmp/out --err /tmp/err cat /proc/loadavg
```

## 세부 사용법

### `-h` `-i` 옵션 사용 예제

-h로 호스트 지정 및 인터렉티브 모드

```bash
./clsh -h "localhost,quokkaandco.dev" -i
using -h option
using -i option
localhost: 
localhost: Linux debian 6.0.0-5-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.0.10-2 (2022-12-01) x86_64
localhost: 
localhost: The programs included with the Debian GNU/Linux system are free software;
localhost: the exact distribution terms for each program are described in the
localhost: individual files in /usr/share/doc/*/copyright.
localhost: 
localhost: Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
localhost: permitted by applicable law.
localhost: You have new mail.
localhost: Last login: Mon Dec 19 23:49:31 2022 from ::1

quokkaandco.dev: 
quokkaandco.dev: Linux debian-rpi4 5.19.0-arm64 #1 SMP PREEMPT Wed Aug 3 21:23:21 KST 2022 aarch64
quokkaandco.dev: 
quokkaandco.dev: The programs included with the Debian GNU/Linux system are free software;
quokkaandco.dev: the exact distribution terms for each program are described in the
quokkaandco.dev: individual files in /usr/share/doc/*/copyright.
quokkaandco.dev: 
quokkaandco.dev: Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
quokkaandco.dev: permitted by applicable law.
quokkaandco.dev: Last login: Mon Dec 19 23:49:31 2022 from 116.126.196.175
quokkaandco.dev: 
quokkaandco.dev: 
quokkaandco.dev: Microsoft(R) Windows 95
quokkaandco.dev:    (C)Copyright Microsoft Corp 1981-1996.
quokkaandco.dev: 
clsh > ^exit
INPUT : exit ()
localhost : clsh node exited with status code (0)
quokkaandco.dev : clsh node exited with status code (0)
```

### '-h'로 호스트 지정 및 지정 명령 실행

```bash
./clsh -h "localhost,quokkaandco.dev" cat /proc/cmdline
using -h option
localhost: 
localhost: BOOT_IMAGE=/boot/vmlinuz-6.0.0-5-amd64 root=UUID=0b383b0c-fe07-49f5-997d-51379cc3e1fc ro quiet splash "acpi_osi=Windows 2015"
localhost : clsh node exited with status code (0)
DISCON : localhost
quokkaandco.dev: 
quokkaandco.dev: BOOT_IMAGE=/boot/vmlinuz-5.19.0-arm64 root=UUID=d9ed9492-31f7-4eb3-ae43-4c38ac12aa3f ro quiet usbhid.mousepoll=0 TERM=linux-16color
quokkaandco.dev : clsh node exited with status code (0)
DISCON : quokkaandco.dev
```