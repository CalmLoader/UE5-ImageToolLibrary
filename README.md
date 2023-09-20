# UE5-ImageToolLibrary
封装在UE4/5中图片操作常用方法

# 常用方法
- 获取大小
  - GetImageResolution：获取硬盘加载图片大小
  - GetTextureResolution：获取UTexture2DDynamic纹理大小

- 加载和保存
  - LoadImageToTexture2D：从硬盘加载图片为UTexture2D
  - LoadImageToTexture2DDy：从硬盘加载图片为UTexture2DDynamic
  - SaveImageFromTexture2D：保存UTexture2D到指定硬盘路径
  - SaveImageFromTexture2DDy：保存UTexture2DDynamic到指定硬盘路径

- RT2D
  - RenderWidget：渲染UWidget为RT2D
  - SaveRenderTarget2D：保存RT2D到硬盘路径
  - RenderWidgetToUTexture2D：渲染UWidget到UTexture2D

- 打包
  - PackTexture：打包纹理

# 使用场景
- 获取大小：从硬盘加载过大图片资源时需要过滤
- 加载和保存：CDN或一些缓存图片资源，需要从硬盘空间进行加载
- RT2D：用于分享时对UMG或部分Widget进行渲染，并保存至硬盘
- 打包：进行简单纹理打包，不用采用额外工具
