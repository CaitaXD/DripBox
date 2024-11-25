import os
import uuid
import sys
from argparse import ArgumentParser

parser = ArgumentParser()
sub_parsers = parser.add_subparsers(dest="command")

run_parser = sub_parsers.add_parser("run")
run_parser.add_argument("--clean", "-c", help="Remove old containers", action="store_true")
run_sub_parsers = run_parser.add_subparsers(dest="mode")
run_server_mode = run_sub_parsers.add_parser("server")
run_client_mode = run_sub_parsers.add_parser("client")
run_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
run_parser.add_argument("port", help="Port to use", type=int, default=25565)
run_parser.add_argument("container_name", help="Container name", type=str, default="dripbox")
run_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

build_parser = sub_parsers.add_parser("build")
build_sub_parsers = build_parser.add_subparsers(dest="mode")
build_server_mode = build_sub_parsers.add_parser("server")
build_client_mode = build_sub_parsers.add_parser("client")
build_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
build_parser.add_argument("port", help="Port to use", type=int, default=25565)
build_parser.add_argument("container_name", help="Container name", type=str, default="dripbox")
build_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

build_run_parser = sub_parsers.add_parser("build-run")
build_run_parser.add_argument("--clean", "-c", help="Remove old containers", action="store_true")
build_run_sub_parsers = build_run_parser.add_subparsers(dest="mode")
build_run_server_mode = build_run_sub_parsers.add_parser("server")
build_run_client_mode = build_run_sub_parsers.add_parser("client")
build_run_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
build_run_parser.add_argument("port", help="Port to use", type=int, default=25565)
build_run_parser.add_argument("container_name", help="Container name", type=str, default="dripbox")
build_run_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

clear_parser = sub_parsers.add_parser("clear")
clear_sub_parsers = clear_parser.add_subparsers(dest="mode")
clear_server_mode = clear_sub_parsers.add_parser("server")
clear_client_mode = clear_sub_parsers.add_parser("client")
clear_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")
clear_client_mode.add_argument("container_name", help="Container name", type=str, default="dripbox")


args = parser.parse_args()
print(args)

if args.command == "build-run" and args.mode == "server":
    container_name = args.container_name.lower().replace(" ", "-")
    if args.clean:
        os.system(f"cd .. && "
                  f"docker rm -f {container_name}-server && "
                  f"docker rmi -f {container_name}-server"
                  )
    os.system(f"cd .. && "
              f"docker build "
              f"-f build/Dockerfile "
              f"-t {container_name}-{args.mode} "
              f"."
              )
    os.system(f"cd .. && "
              f"docker run --name {container_name}-{args.mode} --network bridge -t {container_name}-{args.mode}"
              )
elif args.command == "build-run" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    container_name = args.container_name.lower().replace(" ", "-")
    if args.clean:
        os.system(f"cd .. && "
                  f"docker rm -f {container_name}-{username} && "
                  f"docker rmi -f {container_name}-{username}"
                  )
    os.system(f"cd .. && "
              f"docker build "
              f"-t {container_name}-{username} "
              f"-f build/Dockerfile "
              f"--build-arg KIND='client={username}' "
              f"--build-arg IP={args.ip} "
              f"--build-arg PORT={args.port} "
              f"."
              )
    os.system(f"cd .. && "
              f"docker run "
              f" -i --name {container_name}-{username} "
              f"--network bridge "
              f"-t {container_name}-{username}"
              )

elif args.command == "clear" and args.mode == "server":
    container_name = args.container_name.lower().replace(" ", "-")
    os.system(f"cd .. && "
              f"docker rm -f {container_name}-server && "
              f"docker rmi -f {container_name}-server"
              )

elif args.command == "clear" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    container_name = args.container_name.lower().replace(" ", "-")
    os.system(f"cd .. && "
              f"docker rm -f {container_name}-{username} && "
              f"docker rmi -f {container_name}-{username}"
              )

elif args.command == "build" and args.mode == "server":
    container_name = args.container_name.lower().replace(" ", "-")
    os.system(f"cd .. && "
              f"docker build "
              f"-f build/Dockerfile "
              f"-t {container_name}-{args.mode} "
              f"."
              )

elif args.command == "build" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    container_name = args.container_name.lower().replace(" ", "-")
    os.system(f"cd .. && "
              f"docker build "
              f"-t {container_name}-{username} "
              f"-f build/Dockerfile "
              f"--build-arg KIND='client={username}' "
              f"--build-arg IP={args.ip} "
              f"--build-arg PORT={args.port} "
              f"."
              )

elif args.command == "run" and args.mode == "server":
    container_name = args.container_name.lower().replace(" ", "-")
    if args.clean:
        os.system(f"cd .. && "
                  f"docker rm -f {container_name}-server"
                  )
    os.system(f"docker run -d --name {container_name}-{args.mode} --network bridge -t {container_name}-{args.mode}")

elif args.command == "run" and args.mode == "client":
    username = args.username.lower().replace(" ", "-")
    container_name = args.container_name.lower().replace(" ", "-")
    if args.clean:
        os.system(f"cd .. && "
                  f"docker rm -f {container_name}-{username}"
                  )
    os.system(f"docker run "
              f"-d --name {container_name}-{username} "
              f"--network bridge "
              f"-t {container_name}-{username}"
              )
else:
    raise "Not implemented"
