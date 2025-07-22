document.addEventListener('DOMContentLoaded', function() {
    const searchForm = document.querySelector('form');
    const resultArea = document.getElementById('result');
    
    if (searchForm) {
        searchForm.addEventListener('submit', function(e) {
            e.preventDefault();
            performSearch();
        });
    }
    
    // 执行搜索并显示结果
    function performSearch() {
        const key1 = document.getElementById('key1').value;
        const key2 = document.getElementById('key2').value;
        
        if (!key1 || !key2) {
            showError('请输入班级和关键字');
            return;
        }
        
        // 显示加载状态
        showLoading();
        
        // 发送POST请求到服务器
        fetch('/search', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `key1=${encodeURIComponent(key1)}&key2=${encodeURIComponent(key2)}`
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.text();
        })
        .then(data => {
            displayResults(data);
        })
        .catch(error => {
            showError('搜索失败: ' + error.message);
        });
    }
    
    // 显示加载状态
    function showLoading() {
        resultArea.innerHTML = `
            <div class="loading">
                <img src="../img/loading.gif" alt="加载中" style="width:50px;">
                <p>正在搜索，请稍候...</p>
            </div>
        `;
    }
    
    // 显示错误信息
    function showError(message) {
        resultArea.innerHTML = `
            <div class="error">
                <p style="color:red;">${message}</p>
            </div>
        `;
    }
    
    // 显示搜索结果
    function displayResults(data) {
        // 如果结果是"文件不存在"，显示错误信息
        if (data.trim() === '文件不存在') {
            showError('未找到该班级的信息');
            return;
        }
        
        // 解析结果并显示
        const results = data.split('\n').filter(line => line.trim() !== '');
        
        if (results.length === 0) {
            showError('未找到匹配的记录');
            return;
        }
        
        // 生成结果表格
        let tableHtml = `
            <h3>搜索结果 (班级: ${document.getElementById('key1').value}, 关键字: ${document.getElementById('key2').value})</h3>
            <table class="result-table">
                <thead>
                    <tr>
                        <th>学号</th>
                        <th>姓名</th>
                        <th>性别</th>
                    </tr>
                </thead>
                <tbody>
        `;
        
        results.forEach(line => {
            const [id, name, gender] = line.split(' ');
            tableHtml += `
                <tr>
                    <td>${id}</td>
                    <td>${name}</td>
                    <td>${gender}</td>
                </tr>
            `;
        });
        
        tableHtml += `
                </tbody>
            </table>
        `;
        
        resultArea.innerHTML = tableHtml;
        
        // 添加表格样式
        const style = document.createElement('style');
        style.textContent = `
            .result-table {
                width: 100%;
                border-collapse: collapse;
                margin-top: 20px;
            }
            .result-table th, .result-table td {
                padding: 10px;
                border: 1px solid #ddd;
                text-align: left;
            }
            .result-table th {
                background-color: #f2f2f2;
            }
            .result-table tr:hover {
                background-color: #f5f5f5;
            }
            .loading, .error {
                text-align: center;
                padding: 20px;
            }
        `;
        document.head.appendChild(style);
    }
});
