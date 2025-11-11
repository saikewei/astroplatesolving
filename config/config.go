package config

import (
	"fmt"
	"os"

	"gopkg.in/yaml.v3"
)

// Config 表示应用程序的配置结构
type Config struct {
	Server struct {
		Port int `yaml:"port"`
	} `yaml:"server"`
	Astrometry struct {
		IndexPath string `yaml:"index_path"`
	} `yaml:"astrometry"`
}

var AppConfig Config

// LoadConfig 从指定路径加载配置文件
func LoadConfig(configPath string) error {
	data, err := os.ReadFile(configPath)
	if err != nil {
		return fmt.Errorf("无法读取配置文件: %w", err)
	}

	err = yaml.Unmarshal(data, &AppConfig)
	if err != nil {
		return fmt.Errorf("无法解析配置文件: %w", err)
	}

	return nil
}

// GetIndexPath 返回星历文件路径
func GetIndexPath() string {
	return AppConfig.Astrometry.IndexPath
}

// GetServerPort 返回服务器监听端口
func GetServerPort() string {
	return fmt.Sprintf(":%d", AppConfig.Server.Port)
}
