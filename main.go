package main

import (
	"astro_plate_solving/routes"
)

func main() {
	// 设置路由
	router := routes.SetupRouter()

	// 启动服务器
	router.Run(":8080")
}
