// فایل منیجر حرفه‌ای شبیه اکسپلورر ویندوز
let currentPath = "/";
let selectedFile = null;

// نمایش لیست فایل‌ها و فولدرها
function listFiles(path = "/") {
    fetch(`/list?dir=${encodeURIComponent(path)}`)
        .then(res => res.json())
        .then(data => {
            renderFilesTable(data.files || [], path);
            renderBreadcrumb(path);
        });
}

// رندر جدول فایل‌ها و فولدرها
function renderFilesTable(files, path) {
    const tbody = document.querySelector("#filesTable tbody");
    tbody.innerHTML = "";
    files.forEach(file => {
        const tr = document.createElement("tr");
        tr.innerHTML = `
            <td class="icon">${file.type === 'dir' ? "<i class='bx bx-folder'></i>" : "<i class='bx bx-file'></i>"}</td>
            <td class="file-name" style="cursor:pointer;">${file.name}</td>
            <td>${file.type === 'dir' ? "-" : formatSize(file.size)}</td>
            <td>${file.date || "-"}</td>
            <td class="actions">
                <button class="btn btn-sm btn-primary download-btn" ${file.type === 'dir' ? 'disabled' : ''}><i class='bx bx-download'></i></button>
                <button class="btn btn-sm btn-danger delete-btn"><i class='bx bx-trash'></i></button>
            </td>
        `;
        // انتخاب فایل/پوشه
        tr.querySelector('.file-name').onclick = () => {
            if (file.type === 'dir') {
                currentPath = path.endsWith("/") ? path + file.name : path + "/" + file.name;
                listFiles(currentPath);
            } else {
                selectedFile = path.endsWith("/") ? path + file.name : path + "/" + file.name;
                highlightRow(tr);
            }
        };
        // دانلود فایل
        tr.querySelector('.download-btn').onclick = (e) => {
            e.stopPropagation();
            downloadFile(path, file.name);
        };
        // حذف فایل/پوشه
        tr.querySelector('.delete-btn').onclick = (e) => {
            e.stopPropagation();
            deleteFile(path, file.name, file.type);
        };
        tbody.appendChild(tr);
    });
}

// نمایش مسیر جاری (breadcrumb) با آیکون و جداکننده
function renderBreadcrumb(path) {
    const bc = document.getElementById('breadcrumb-path');
    const parts = path.split('/').filter(Boolean);
    let html = '';
    if(parts.length === 0) html = '<span style="color:#00ffc3">/</span>';
    else {
        html = '<span style="color:#00ffc3; cursor:pointer;" onclick="goToPath(0)"><i class="bx bx-folder"></i> root</span>';
        parts.forEach((part, i) => {
            html += ` <span style='color:#888'>/</span> <span style='cursor:pointer;' onclick='goToPath(${i+1})'><i class="bx bx-folder"></i> ${part}</span>`;
        });
    }
    bc.innerHTML = html;
}
function goToPath(idx) {
    const parts = currentPath.split('/').filter(Boolean);
    const newPath = '/' + parts.slice(0, idx).join('/');
    currentPath = newPath === '/' ? '/' : newPath;
    listFiles(currentPath);
}

// فرمت سایز فایل
function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
    return (bytes/1024/1024).toFixed(1) + ' MB';
}

// هایلایت ردیف انتخاب شده
function highlightRow(tr) {
    document.querySelectorAll('#filesTable tr').forEach(row => row.classList.remove('table-active'));
    tr.classList.add('table-active');
}

// آپلود فایل
const uploadBtn = document.getElementById('uploadBtn');
const uploadInput = document.getElementById('uploadInput');
uploadBtn.onclick = () => uploadInput.click();
uploadInput.onchange = () => {
    const files = uploadInput.files;
    if (!files.length) return;
    Array.from(files).forEach(file => {
        const formData = new FormData();
        formData.append('file', file);
        let dest = currentPath;
        if(!dest.endsWith('/')) dest += '/';
        formData.append('path', dest);
        fetch('/upload', { method: 'POST', body: formData })
            .then(() => listFiles(currentPath));
    });
};

// دانلود فایل
function downloadFile(path, name) {
    const url = `/download?file=${encodeURIComponent(path.endsWith("/") ? path + name : path + "/" + name)}`;
    window.open(url, '_blank');
}

document.getElementById('downloadBtn').onclick = () => {
    if (selectedFile) downloadFile('', selectedFile.replace(/^\//, ''));
};

// حذف فایل/پوشه
function deleteFile(path, name, type) {
    if (!confirm('آیا مطمئن هستید؟')) return;
    const fullPath = path.endsWith("/") ? path + name : path + "/" + name;
    if(type === 'dir') {
        fetch(`/delete?file=${encodeURIComponent(fullPath)}&recursive=1`)
            .then(() => listFiles(currentPath));
    } else {
        fetch(`/delete?file=${encodeURIComponent(fullPath)}`)
            .then(() => listFiles(currentPath));
    }
}
document.getElementById('deleteBtn').onclick = () => {
    if (selectedFile) deleteFile('', selectedFile.replace(/^\//, ''), 'file');
};

// ایجاد پوشه جدید
const newFolderBtn = document.getElementById('newFolderBtn');
newFolderBtn.onclick = () => {
    const folderName = prompt('نام پوشه جدید:');
    if (!folderName) return;
    fetch(`/mkdir?dir=${encodeURIComponent(currentPath.endsWith("/") ? currentPath + folderName : currentPath + "/" + folderName)}`)
        .then(() => listFiles(currentPath));
};

// رفرش
const refreshBtn = document.getElementById('refreshBtn');
refreshBtn.onclick = () => listFiles(currentPath);

// دانلود config.json
const downloadConfigBtn = document.getElementById('downloadConfigBtn');
downloadConfigBtn.onclick = () => {
    window.open('/download?file=/config.json', '_blank');
};

// آپلود config.json
const uploadConfigBtn = document.getElementById('uploadConfigBtn');
uploadConfigBtn.onclick = () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json,application/json';
    input.onchange = () => {
        const file = input.files[0];
        if (!file) return;
        const formData = new FormData();
        formData.append('file', file);
        formData.append('path', '/');
        fetch('/upload', { method: 'POST', body: formData })
            .then(() => listFiles(currentPath));
    };
    input.click();
};

// بارگذاری اولیه
window.onload = () => listFiles(currentPath); 