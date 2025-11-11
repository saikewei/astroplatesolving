package handlers

import (
	"astro_plate_solving/config"
	"astro_plate_solving/models"
	"astro_plate_solving/utils"
	"io"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

// HealthCheck - 健康检查
func HealthCheck(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status":  "ok",
		"message": "服务运行正常",
	})
}

// - "image": 图像文件
// - "focal_length": 相机焦距 (毫米)
func SolveImageHandler(c *gin.Context) {
	// 1. 解析焦距
	focalLengthStr := c.PostForm("focal_length")
	if focalLengthStr == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "缺少 'focal_length' 字段"})
		return
	}
	focalLength, err := strconv.ParseFloat(focalLengthStr, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的 'focal_length' 值，必须是数字"})
		return
	}

	// 2. 解析上传的图像文件
	file, err := c.FormFile("image")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "缺少 'image' 文件"})
		return
	}

	// 3. 读取文件内容
	src, err := file.Open()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "无法打开上传的文件"})
		return
	}
	defer src.Close()

	imageData, err := io.ReadAll(src)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "无法读取文件内容"})
		return
	}

	// 4. 从配置文件中获取索引文件路径
	indexPath := config.GetIndexPath()

	// 5. 调用核心解析函数
	solution, err := utils.Solve(imageData, indexPath, focalLength)
	if err != nil {
		// 解析失败
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "天体解析失败",
			"details": err.Error(),
		})
		return
	}

	var jsonSolution models.Solution
	jsonSolution.FieldWidth = solution.FieldWidth
	jsonSolution.FieldHeight = solution.FieldHeight
	jsonSolution.Ra = solution.RA
	jsonSolution.Dec = solution.Dec
	jsonSolution.Orientation = solution.Orientation
	jsonSolution.Pixscale = solution.Pixscale
	jsonSolution.RaError = solution.RAError
	jsonSolution.DecError = solution.DecError
	// 6. 返回成功结果
	c.JSON(http.StatusOK, jsonSolution)
}
