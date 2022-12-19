import asyncio
import node


async def main():
    ssh_node = node.Node()
    await ssh_node.start_node()
    ssh_manager = node.NodeManager()
    await ssh_manager.add_node(ssh_node)
    await ssh_node.node_watch()
    await ssh_node.stop_node()
    await ssh_manager.stop()

if __name__ == "__main__":
    asyncio.run(main())
