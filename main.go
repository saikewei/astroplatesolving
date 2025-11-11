package main

import (
	"astro_plate_solving/config"
	"astro_plate_solving/routes"
	"log"
)

func main() {
	// 加载配置文件
	if err := config.LoadConfig("config.yaml"); err != nil {
		log.Fatalf("加载配置文件失败: %v", err)
	}
	// 设置路由
	router := routes.SetupRouter()

	// 启动服务器
	port := config.GetServerPort()
	log.Printf("服务器启动在端口 %s", port)
	router.Run(port)
}
