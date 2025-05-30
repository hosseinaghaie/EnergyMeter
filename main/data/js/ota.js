// ファイル: ota.js - بروزرسانی OTA

document.addEventListener('DOMContentLoaded', function() {
    const firmwareFileInput = document.getElementById('firmwareFile');
    const uploadFirmwareButton = document.getElementById('uploadFirmware');
    const firmwareProgress = document.getElementById('firmwareProgress');
    const firmwareStatus = document.getElementById('firmwareStatus');
    
    const littlefsFileInput = document.getElementById('littlefsFile');
    const uploadLittleFSButton = document.getElementById('uploadLittleFS');
    const littlefsProgress = document.getElementById('littlefsProgress');
    const littlefsStatus = document.getElementById('littlefsStatus');
    
    // بروزرسانی فریم‌ور
    if (uploadFirmwareButton) {
        uploadFirmwareButton.addEventListener('click', function() {
            const file = firmwareFileInput.files[0];
            if (!file) {
                firmwareStatus.textContent = "لطفاً یک فایل فریم‌ور انتخاب کنید.";
                return;
            }
            
            // بررسی اندازه فایل
            if (file.size > 2000000) {
                firmwareStatus.textContent = "فایل بسیار بزرگ است. حداکثر 2MB مجاز است.";
                return;
            }
            
            // شروع آپلود
            firmwareStatus.textContent = "در حال آپلود فریم‌ور...";
            
            const xhr = new XMLHttpRequest();
            const formData = new FormData();
            
            // نکته مهم: در اینجا از نام پارامتر خاصی استفاده نمی‌کنیم
            // فایل را مستقیماً به درخواست اضافه می‌کنیم
            formData.append(file.name, file);
            
            xhr.open('POST', '/update', true);
            
            // نمایش پیشرفت
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    firmwareProgress.style.width = percentComplete + '%';
                    firmwareStatus.textContent = `آپلود: ${Math.round(percentComplete)}%`;
                }
            };
            
            // بررسی وضعیت پایان آپلود
            xhr.onload = function() {
                if (xhr.status === 200) {
                    firmwareStatus.textContent = "بروزرسانی موفق. دستگاه در حال راه‌اندازی مجدد...";
                    firmwareProgress.style.width = '100%';
                } else {
                    firmwareStatus.textContent = "خطا در بروزرسانی: " + xhr.statusText;
                    firmwareProgress.style.width = '0%';
                }
            };
            
            // مدیریت خطاها
            xhr.onerror = function() {
                firmwareStatus.textContent = "خطا در ارتباط با سرور.";
                firmwareProgress.style.width = '0%';
            };
            
            xhr.send(formData);
        });
    }
    
    // آپلود LittleFS
    if (uploadLittleFSButton) {
        uploadLittleFSButton.addEventListener('click', function() {
            const file = littlefsFileInput.files[0];
            if (!file) {
                littlefsStatus.textContent = "لطفاً یک فایل LittleFS انتخاب کنید.";
                return;
            }
            
            // بررسی اندازه فایل
            if (file.size > 2000000) {
                littlefsStatus.textContent = "فایل بسیار بزرگ است. حداکثر 2MB مجاز است.";
                return;
            }
            
            // شروع آپلود
            littlefsStatus.textContent = "در حال آپلود فایل LittleFS...";
            
            const xhr = new XMLHttpRequest();
            const formData = new FormData();
            
            // نکته مهم: فایل را با نام اصلی آن اضافه می‌کنیم
            // و پارامتر path را اضافه می‌کنیم
            formData.append(file.name, file);
            formData.append('path', '/');
            
            xhr.open('POST', '/upload', true);
            
            // نمایش پیشرفت
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    littlefsProgress.style.width = percentComplete + '%';
                    littlefsStatus.textContent = `آپلود: ${Math.round(percentComplete)}%`;
                }
            };
            
            // بررسی وضعیت پایان آپلود
            xhr.onload = function() {
                if (xhr.status === 200) {
                    littlefsStatus.textContent = "آپلود موفق. صفحه را رفرش کنید.";
                    littlefsProgress.style.width = '100%';
                    // رفرش صفحه بعد از 3 ثانیه
                    setTimeout(() => {
                        window.location.reload();
                    }, 3000);
                } else {
                    littlefsStatus.textContent = "خطا در آپلود: " + xhr.statusText;
                    littlefsProgress.style.width = '0%';
                }
            };
            
            // مدیریت خطاها
            xhr.onerror = function() {
                littlefsStatus.textContent = "خطا در ارتباط با سرور.";
                littlefsProgress.style.width = '0%';
            };
            
            xhr.send(formData);
        });
    }
}); 