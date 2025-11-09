package routes

import (
	"astro_plate_solving/handlers"

	"github.com/gin-gonic/gin"
)

// SetupRouter - 配置路由
func SetupRouter() *gin.Engine {
	// 创建 Gin 路由器
	router := gin.Default()

	// 健康检查端点
	router.GET("/health", handlers.HealthCheck)

	// API 路由组
	api := router.Group("/api")
	{
		api.POST("/solve", handlers.SolveImageHandler)
	}

	return router
}
