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
run_dns_mode = run_sub_parsers.add_parser("dns")

run_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
run_parser.add_argument("port", help="Port to use", type=int, default=25565)
run_parser.add_argument("container_name", help="Container name", type=str, default="dripbox")
run_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

build_parser = sub_parsers.add_parser("build")
build_sub_parsers = build_parser.add_subparsers(dest="mode")
build_server_mode = build_sub_parsers.add_parser("server")
build_client_mode = build_sub_parsers.add_parser("client")
build_dns_mode = build_sub_parsers.add_parser("dns")

build_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
build_parser.add_argument("port", help="Port to use", type=int, default=25565)
build_parser.add_argument("container_name", help="Container name", type=str, default="dripbox")
build_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

build_run_parser = sub_parsers.add_parser("build-run")
build_run_parser.add_argument("--clean", "-c", help="Remove old containers", action="store_true")
build_run_sub_parsers = build_run_parser.add_subparsers(dest="mode")
build_run_server_mode = build_run_sub_parsers.add_parser("server")
build_run_client_mode = build_run_sub_parsers.add_parser("client")
build_run_dns_mode = build_run_sub_parsers.add_parser("dns")
build_run_parser.add_argument("ip", help="IP to use", type=str, default="0.0.0.0")
build_run_parser.add_argument("port", help="Port to use", type=int, default=25565)
build_run_parser.add_argument("container_name", help="Container name", type=str, default="dripbox")
build_run_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")

clear_parser = sub_parsers.add_parser("clear")
clear_sub_parsers = clear_parser.add_subparsers(dest="mode")
clear_server_mode = clear_sub_parsers.add_parser("server")
clear_client_mode = clear_sub_parsers.add_parser("client")
clear_dns_mode = clear_sub_parsers.add_parser("dns")
clear_client_mode.add_argument("username", help="Username to use", type=str, default="General Kenoby")
clear_client_mode.add_argument("container_name", help="Container name", type=str, default="dripbox")


args = parser.parse_args()
print(args)

name = args.username.lower().replace(" ", "-") if args.mode == "client" else args.mode
container_name = f"{args.container_name.lower().replace(" ", "-")}-{name}"
kind_arg = f'client={name}' if args.mode == "client" else name

if args.clean and (args.command == "build-run" or args.command == "build"):
    os.system(f"cd .. && docker rm -f {container_name} && docker rmi -f {container_name}")

elif args.clean and args.command == "run":
    os.system(f"cd .. && docker rm -f {container_name}")

if args.command == "build-run" or args.command == "build":
    os.system(f"cd .. && "
              f"docker build "
              f"-t {container_name} "
              f"-f build/Dockerfile "
              f"--build-arg KIND={kind_arg} "
              f"--build-arg IP={args.ip} "
              f"--build-arg PORT={args.port} "
              f".")

if args.command == "build-run" or args.command == "run":
    os.system(f"cd .. && docker run -i --name {container_name} --network bridge -t {container_name}")

if args.command == "clear":
    os.system(f"cd .. && docker rm -f {container_name} && docker rmi -f {container_name} ")