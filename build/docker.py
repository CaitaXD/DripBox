import os
import uuid
import sys
from argparse import ArgumentParser

parser = ArgumentParser()
sub_parsers = parser.add_subparsers(dest="command")

run_parser = sub_parsers.add_parser("run")
run_sub_parsers = run_parser.add_subparsers(dest="mode")
run_server_mode = run_sub_parsers.add_parser("server")
run_client_mode = run_sub_parsers.add_parser("client")
run_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

build_parser = sub_parsers.add_parser("build")
build_sub_parsers = build_parser.add_subparsers(dest="mode")
build_server_mode = build_sub_parsers.add_parser("server")
build_client_mode = build_sub_parsers.add_parser("client")
build_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
build_parser.add_argument("port", help="Port to use", type=int, default=25565)
build_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

build_run_parser = sub_parsers.add_parser("build-run")
build_run_parser.add_argument("--clean", "-c", help="Remove old containers", action="store_true")
build_run_sub_parsers = build_run_parser.add_subparsers(dest="mode")
build_run_server_mode = build_run_sub_parsers.add_parser("server")
build_run_client_mode = build_run_sub_parsers.add_parser("client")
build_run_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
build_run_parser.add_argument("port", help="Port to use", type=int, default=25565)
build_run_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

clear_parser = sub_parsers.add_parser("clear")
clear_sub_parsers = clear_parser.add_subparsers(dest="mode")
clear_server_mode = clear_sub_parsers.add_parser("server")
clear_client_mode = clear_sub_parsers.add_parser("client")
clear_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")


args = parser.parse_args()
print(args)

if args.command == "build-run" and args.mode == "server":
    if args.clean:
        os.system(f"cd .. && "
                  f"docker rm -f dripbox-server && "
                  f"docker rmi -f dripbox-server"
                  )
    os.system(f"cd .. && "
              f"docker build "
              f"-f build/Dockerfile "
              f"-t dripbox-{args.mode} "
              f"."
              )
    os.system(f"cd .. && "
              f"docker run -d --name dripbox-{args.mode} --network bridge -t dripbox-{args.mode}"
              )
elif args.command == "build-run" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    if args.clean:
        os.system(f"cd .. && "
                  f"docker rm -f dripbox-{username} && "
                  f"docker rmi -f dripbox-{username}"
                  )
    os.system(f"cd .. && "
              f"docker build "
              f"-t dripbox-{username} "
              f"-f build/Dockerfile "
              f"--build-arg KIND='client={username}' "
              f"--build-arg IP={args.ip} "
              f"--build-arg PORT={args.port} "
              f"."
              )
    os.system(f"cd .. && "
              f"docker run "
              f"-d --name dripbox-{username} "
              f"--network bridge "
              f"-t dripbox-{username}"
              )

elif args.command == "clear" and args.mode == "server":
    os.system(f"cd .. && "
              f"docker rm -f dripbox-server && "
              f"docker rmi -f dripbox-server"
              )

elif args.command == "clear" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    os.system(f"cd .. && "
              f"docker rm -f dripbox-{username} && "
              f"docker rmi -f dripbox-{username}"
              )

elif args.command == "build" and args.mode == "server":
    os.system(f"cd .. && "
              f"docker build "
              f"-f build/Dockerfile "
              f"-t dripbox-{args.mode} "
              f"."
              )

elif args.command == "build" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    os.system(f"cd .. && "
              f"docker build "
              f"-t dripbox-{username} "
              f"-f build/Dockerfile "
              f"--build-arg KIND='client={username}' "
              f"--build-arg IP={args.ip} "
              f"--build-arg PORT={args.port} "
              f"."
              )

elif args.command == "run" and args.mode == "server":
    os.system(f"docker run -d --name dripbox-{args.mode} --network bridge -t dripbox-{args.mode}")

elif args.command == "run" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    os.system(f"docker run "
              f"-d --name dripbox-{username} "
              f"--network bridge "
              f"-t dripbox-{username}"
              )
else:
    raise "Not implemented"
