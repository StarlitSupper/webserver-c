document.addEventListener('DOMContentLoaded', function() {
    const searchForm = document.getElementById('searchform');
    const resultArea = document.getElementById('result');
    
    if (searchForm && resultArea) {
        searchForm.addEventListener('submit', function(e) {
            e.preventDefault();
            performSearch();
        });
    }
    
    // 将辅助函数移到事件监听器内部
    function performSearch() {
        const key1 = document.getElementById('key1').value.trim();
        const key2 = document.getElementById('key2').value.trim();
        
        if (!key1 || !key2) {
            showError('请输入班级和关键字');
            return;
        }
        
        showLoading();
        
        fetch('/search', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `key1=${encodeURIComponent(key1)}&key2=${encodeURIComponent(key2)}`
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`服务器错误: ${response.status}`);
            }
            
            const contentType = response.headers.get('Content-Type');
            if (!contentType || !contentType.includes('text/plain')) {
                throw new Error(`意外的响应类型: ${contentType}`);
            }
            
            return response.text();
        })
        .then(displayResults)
        .catch(error => showError(`搜索失败: ${error.message}`));
    }
    
    function showLoading() {
        resultArea.innerHTML = `
            <div class="loading">
                <p>正在搜索，请稍候...</p>
            </div>
        `;
    }
    
    function showError(message) {
        resultArea.innerHTML = `
            <div class="error" style="color: red; padding: 10px;">
                ${message}
            </div>
        `;
    }
    
    function displayResults(data) {
        if (data === '文件不存在') {
            showError('未找到该班级的信息文件');
            return;
        }
        
        if (!data.trim()) {
            showError('未找到匹配的记录');
            return;
        }
        
        const lines = data.trim().split('\n');
        let tableHtml = `
            <div class="result-table" style="margin-top: 20px;">
                <h3>搜索结果（${lines.length}条）</h3>
                <table border="1" style="width:100%; border-collapse: collapse;">
                    <tr style="background-color: #f0f0f0;">
                        <th style="padding: 8px; text-align: left;">学号</th>
                        <th style="padding: 8px; text-align: left;">姓名</th>
                        <th style="padding: 8px; text-align: left;">性别</th>
                    </tr>
        `;
        
        lines.forEach(line => {
            const [id, name, gender] = line.split(/\s+/);
            tableHtml += `
                <tr>
                    <td style="padding: 8px;">${id || ''}</td>
                    <td style="padding: 8px;">${name || ''}</td>
                    <td style="padding: 8px;">${gender || ''}</td>
                </tr>
            `;
        });
        
        tableHtml += `</table></div>`;
        resultArea.innerHTML = tableHtml;
    }
});
